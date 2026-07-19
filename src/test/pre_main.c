
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

#if defined(__riscv)
#include <riscv_vector.h>
#endif

extern long local_pid;
extern int thread_set;
extern int thread_set_errno;

bool set_ai_thread();

int main(int argc, char **argv, char **envs) {

    printf("Got pid: %ld\n", local_pid);
    printf("There are %d arguments:\n", argc);
    for (size_t it = 0; it < argc; it++) {
        puts(argv[it]);
        puts("\n");
    };

    puts("Meanwhile, in the environment variables: ...\n");
    char *env_ptr = *envs;
    /*while (env_ptr != NULL) {
        puts(env_ptr);
        puts("\n");
        env_ptr++;
    }*/

    printf("Set AI thread? %b", set_ai_thread());
    printf("thread_set = %d, thread_set_errno = %d\n", thread_set, thread_set_errno);
    if (thread_set == -1) {
        printf(
            "Didn't set to an AI thread.\n"
            "Error number: %d\n"
            "Error message: %s\n",
            thread_set_errno,
            strerror(thread_set_errno)
        );
    }


#if defined(__riscv)
    unsigned long vec_len = __riscv_vlenb();
    printf("vector buffer is %ld bits\n", vec_len * 8);
#endif

    return 0;
}

