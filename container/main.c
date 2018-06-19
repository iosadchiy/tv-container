#define _GNU_SOURCE

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdarg.h>

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

// Wrapper around system() to allow sprintf-like formatting
int systemf(const char *cmd_format, ...) {
    char cmd[1000];

    va_list va;
    va_start(va, cmd_format);
    vsprintf(cmd, cmd_format, va);
    va_end(va);

    system(cmd);
}

// The body of the container:
// do the setup and exec bash
static int child_fn() {
    // Mount /proc to filter the processes visible to the container
    printf("Clone internal PID: %ld\n", (long) getpid());
    system("mount -t proc proc /proc --make-private");
    system("ps aux");

    printf("Guest network namespace:\n");
    system("ip link");
    printf("\n\n");

    // Setup network inside the container
    // It can ping host: `ping 10.1.1.1`
    systemf("ifconfig veth1 10.1.1.2/24 up");

    // Container's /home is actually inside the /tmp/tv_filesystem file
    system("mount /dev/loop1 /home");

    execl("/bin/bash", "bash", "-i", NULL);
    return EXIT_SUCCESS;
}

int main() {
    // Prepare the container filesystem
    char *fs = "/tmp/tv_filesystem";
    systemf("dd if=/dev/zero of=%s bs=1M count=1", fs);
    systemf("losetup /dev/loop1 %s", fs);
    system("mkfs.ext2 /dev/loop1");

    // Clone with namespaces for processes, network and mounts
    pid_t child_pid =
            clone(child_fn, child_stack + STACK_SIZE, CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | SIGCHLD, NULL);
    printf("clone() = %ld\n", (long) child_pid);

    // Setup network: veth0 on the host and veth1 in the container
    systemf("ip link add name veth0 type veth peer name veth1 netns %d", child_pid);
    systemf("ifconfig veth0 10.1.1.1/24 up");

    // Add container to the CPU cgroup named "demo" to limit it to 50% of all the CPU resources
    system("mkdir /sys/fs/cgroup/cpu/demo");
    system("echo 50000 > /sys/fs/cgroup/cpu/demo/cpu.cfs_quota_us");
    system("echo 100000 > /sys/fs/cgroup/cpu/demo/cpu.cfs_period_us");
    systemf("echo %d > /sys/fs/cgroup/cpu/demo/tasks", child_pid);

    // Wait until the container exits
    waitpid(child_pid, NULL, 0);

    // Free resources
    system("umount /proc");
    system("umount /home");
    system("losetup -D");
    system("rmdir /sys/fs/cgroup/cpu/demo");

    return EXIT_SUCCESS;
}
