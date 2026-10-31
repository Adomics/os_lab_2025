
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        printf("Child process (PID %ld) exits.\n", (long)getpid());
        fflush(stdout);
        _exit(0);
    }

    printf("Parent PID: %ld, Child PID: %ld\n",
           (long)getpid(), (long)pid);
    fflush(stdout);

    sleep(1);

    char command[128];
    snprintf(command, sizeof(command),
             "ps -o pid,ppid,stat,cmd -p %ld", (long)pid);

    printf("\nBefore waitpid():\n");
    fflush(stdout);
    system(command);

    printf("\nWaiting 10 seconds...\n");
    fflush(stdout);
    sleep(10);

    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    printf("\nAfter waitpid():\n");
    fflush(stdout);
    system(command);

    return 0;
}
