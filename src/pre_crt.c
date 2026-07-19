// Pre-CRT code here. This file uses the Linux No LibC environment.
// Absolutely NO C RUNTIME CALLS in this file.

#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <stdint.h>

long local_err = 0;
long local_pid = 0;

int thread_set = 0;
int thread_set_errno;

bool configured_for_ai_thread() {
    return (thread_set > 0);
}

// We'll need our own syscall here
// syscall(int num, arg1 = 0, arg2 = 0, arg3 = 0, arg4 = 0, arg5 = 0, arg6 = 0)
// x64:
// mov num, %rax
// mov arg1, %rdi
// mov arg2, %rsi
// mov arg3, %rdx
// mov arg4, %r10
// mov arg5, %r8
// mov arg6, %r9
// syscall
// return is %rax, error in %rdx

// ARM64:
// mov num, %w8
// mov arg1, %r0
// mov arg2, %r1
// mov arg3, %r2
// mov arg4, %r3
// mov arg5, %r4
// mov arg6, %r5
// svc #0
// return is %x0, error in %x1

// RISC-V:
// mov num, %a7
// mov arg1, %a0
// mov arg2, %a1
// mov arg3, %a2
// mov arg4, %a3
// mov arg5, %a4
// mov arg6, %a5
// ecall
// return is %a0, error value in %a1

// With much respect to Felix Cloutier:
// https://www.felixcloutier.com/documents/gcc-asm.html
// All this assembly's great, but it's only valid for x64.
// Next time: a RISC-V port!

#if defined(__x86_64__)
__attribute__((always_inline))
inline
int
getpid() {
    int retval = -1;
    asm volatile(
        "movl %[getpid], %%eax \n "
        "syscall"
        : "+a"(retval) // output
        : [getpid]"i"(SYS_getpid)); // input
    return retval;
}
#elif defined (__riscv)
__attribute__((always_inline))
inline
int
getpid() {
    register int r_a0 asm ("a0") = 0;
    asm volatile (
        "li a7, %[getpid] \n"
        "ecall"
        : "=r"(r_a0)
        : [getpid]"i"(SYS_getpid) //
    );
    return r_a0;
}
#elif defined(__aarch64__)
#error "Not supported yet."
#endif

#if defined(__x86_64__)
__attribute__((always_inline))
inline
int
openat(char *path, int flags, int mode) {
    int retval;
    int err;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    register int mode_arg asm ("r10") = mode;
#pragma GCC diagnostic pop
    asm volatile (
        "movq %[fdl],  %%rdi \n"
        "movq %[openat], %%rax \n"
        "syscall"
        : "=a"(retval), "=d"(err)
        : [fdl]"i"(AT_FDCWD), "S"(path), "d"(flags), [openat]"i"(SYS_openat)
        : "rcx", "r11", "memory"
    );
    if (retval == -1) {
        local_err = err;
    }
    return retval;
}
#elif defined (__riscv)
__attribute__((always_inline))
inline
int
openat (char *path, int flags, int mode) {
    register intptr_t r_a0 asm ("a0") = AT_FDCWD;
    register intptr_t r_a1 asm ("a1") = (intptr_t)path;
    register int flags_arg asm ("a2") = flags;
    register int mode_arg asm ("a3") = mode;
    asm volatile (
        "li a0, %[fdl] \n"
        "li a7, %[openat] \n"
        "ecall"
        : "=r"(r_a0), "=r"(r_a1)
        : [fdl]"i"(AT_FDCWD), [openat]"i"(SYS_openat)
        : "cc", "memory"
    );
    // r_a0 is now retval
    // r_a1 is now err
    if (r_a0 == -1) {
        local_err = (long)r_a1;
    }
    return (int)r_a0;
}
#endif

#if defined(__x86_64__)
__attribute__((always_inline))
inline
int
write(int fp, void *data, int len) {
    int retval = 0;
    int err = 0;
    asm volatile (
        "movq %[write], %%rax \n"
        "syscall"
        : "=a"(retval), "=d"(err) // EAX gets written bytes, EDX gets err
        : "D"(fp), "S"(data), "d"(len), [write]"i"(SYS_write) // EDI: fp, ESI: data, edx: len
        : "rcx", "r11", "memory"
    );
    if (retval == -1) {
        local_err = err;
    }
    return retval;
}
#elif defined(__riscv)
__attribute((always_inline))
inline
int
write(int fp, void *data, int len) {
    register intptr_t r_a0 asm ("a0") = fp;
    register intptr_t r_a1 asm ("a1") = (intptr_t)data;
    register intptr_t r_a2 asm ("a2") = len;
    asm volatile (
        "li a7, %[write] \n"
        "ecall"
        : "=r"(r_a0), "=r"(r_a1)
        : [write]"i"(SYS_write)
        : "cc", "memory"
    );
    if (r_a0 == -1) {
        local_err = (int)r_a1;
    }
    return (int)r_a0;
}
#endif

#if defined(__x86_64__)
__attribute__((always_inline))
inline
int
close(int fp) {
    int retval = 0;
    int err = 0;
    asm volatile (
        "movq %[close], %%rax \n"
        "syscall"
        : "=a"(retval), "=d"(err) // eax: 0 or -1, edx: err on -1 in eax
        : "D"(fp), [close]"i"(SYS_close)
        : "rcx", "r11", "memory"
    );
    if (retval == -1) {
        local_err = err;
    }
    return retval;
}
#elif defined(__riscv)
__attribute__((always_inline))
inline
int
close(int fp) {
    register intptr_t r_a0 asm ("a0") = fp;
    register intptr_t r_a1 asm ("a1") = 0;
    asm volatile (
        "li a7, %[close] \n"
        "ecall"
        : "=r"(r_a0), "=r"(r_a1)
        : "r"(fp), [close]"i"(SYS_close)
        : "cc", "memory"
    );
    if (r_a0 == -1) {
        local_err = (int)r_a1;
    }
    return (int)r_a0;
}
#endif

char *
write_decimal(int num, char *buffer, unsigned int buffer_len, unsigned int *str_len) {
    int test_num = num;
    unsigned int position = buffer_len - 1;

    // Ensure any 32-bit PID can fit. 64-bit ... eh.
    if (buffer_len  < 11) {
        return 0;
    }

    // we're going to walk backwards in
    // the buffer. Trust me.
    buffer[position] = 0;
    position--;
    buffer[position] = '\n';
    position--;
    *str_len = 1;

    while (test_num != 0) {
        position--;
        (*str_len)++;
        int rem = test_num % 10;
        int div = test_num / 10;
        buffer[position] = '0' + rem;
        test_num = div;
    }
    return buffer + position;
}

// https://github.com/brucehoult/k3_ai/blob/main/aix.S
// https://github.com/c3rb3ru5d3d53c/c3rb3ru5d3d53c.github.io/blob/master/content/posts/docs/hooking-libc.en.md.md

static
void
preinit_hook(void)
{
    long pid = getpid();
    local_pid = pid;
    char pid_buffer[17] = {};
    unsigned int pid_len = 0;
    char *pid_str = write_decimal(pid, pid_buffer, sizeof(pid_buffer), &pid_len);
    // We know the buffer's large enough, otherwise I'd do a null check.
    int ai_fd = openat("/proc/set_ai_thread", O_WRONLY, 0);
    if (ai_fd < 0) {
        // Not on a system with that capability?
        // No worries!
        thread_set_errno = local_err;
        return;
    }

    thread_set = write(ai_fd, pid_str, pid_len);
    if (thread_set < 0) {
        thread_set_errno = -thread_set;
        // Continue on to close anyways.
    }

    close(ai_fd);

    return;
}

__attribute__((section(".preinit_array")))
void (*__preinit_hook_ptr)(void) = preinit_hook;
