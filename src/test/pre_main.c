
#include <unistd.h>
#include <errno.h>

#include <stdio.h>


// #include <riscv_vector.h>

extern long local_pid;
extern long thread_set;
extern int thread_set_errno;

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

    if (thread_set == -1) {
        printf("Didn't set to an AI thread. Error number: %d\n", thread_set_errno);
    }

    return 0;
}

