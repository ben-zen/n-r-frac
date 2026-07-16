
#include <unistd.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <fcntl.h>

#include <riscv_vector.h>

#include <linux-syscalls.h>

int main(size_t argc, char **argv, char **) {
    printf("Vector buffer width: %d bits",
           (unsigned int)__riscv_vlenb() * 8);
}

static int (*main_orig)(int, char **, char **);

int main_hook(size_t argc, char **argv, char **envs) {

    printf("There are %u arguments:", argc);
    for (size_t it = 0; it < argc; it++) {
        puts(argv[it]);
        putc('\n');
    };

    puts("Meanwhile, in the environment variables: ...\n");
    char *env_ptr = envs;
    while (env_ptr != NULL) {
        puts(env_ptr);
        env_ptr++;
    }

    return main_orig(argc, argv, envs);
}

// Absolutely NO C STANDARD LIBRARY below this point.

char *
write_decimal(int num, char *buffer, size_t buffer_len, size_t *str_len) {
    int test_num = num;
    size_t position = buffer_len - 1; // we're going to walk backwards in
    // the buffer. Trust me.
    buffer[position] = 0;
    position--;
    str_len = 0;
    // ensure buffer_len is high enough.
    while (test_num != 0) {
        int rem = test_num % 10;
        int div = test_num / 10;
        buffer[position] = '0' + rem;
        test_num = div;
        position--;
        str_len++;
    }
    return buffer + position;
}

// https://github.com/brucehoult/k3_ai/blob/main/aix.S
// https://github.com/c3rb3ru5d3d53c/c3rb3ru5d3d53c.github.io/blob/master/content/posts/docs/hooking-libc.en.md.md

int
__libc_start_main(
    int (*main)(int, char **, char **),
    int argc,
    char **argv,
    int (*init)(int, char **, char **),
    void (*fini)(void),
    void (*rtld_fini)(void),
    void *stack_end) {

    // Stash main.
    main_orig = main;

    long pid = sys_getpid();
    char pid_buffer[16] = {};
    size_t pid_len = 0;
    char * pid_str = write_decimal(pid, pid_buffer, sizeof(pid_buffer), &pid_len);
    int ai_fd = sys_openat(AT_FDCWD, "/proc/set_ai_thread", w, 0600);
    if (ai_fd != -1) {
        // Not on a system with that capability?
        // No worries!
        sys_write(ai_fd, pid_str, pid_len);
        sys_close(ai_fd);
    }

     typeof(&__libc_start_main) orig = dlsym(RTLD_NEXT, "__libc_start_main");
     return orig(main_hook, argc, argv, init, fini, rtld_fini, stack_end);
}


https://c3rb3ru5d3d53c.github.io/2023/02/hooking-libc.en.md/
