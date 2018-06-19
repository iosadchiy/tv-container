#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

static int child_fn() {
    printf("Clone internal PID: %ld\n", (long) getpid());
    execl("/bin/bash", "bash", "-i");
    return EXIT_SUCCESS;
}

int main() {
    pid_t child_pid =
            clone(child_fn, child_stack + STACK_SIZE, CLONE_NEWPID | SIGCHLD, NULL);
    printf("clone() = %ld\n", (long) child_pid);
    waitpid(child_pid, NULL, 0);
    return EXIT_SUCCESS;
}