#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main() {
    printf("=== System Call Interception Test Program ===\n");
    printf("This program will create several processes to test fork/exit interception\n\n");
    
    printf("Creating 3 child processes...\n");
    
    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            printf("Child %d (PID: %d) starting...\n", i+1, getpid());
            sleep(1);
            printf("Child %d (PID: %d) exiting...\n", i+1, getpid());
            exit(0);
        } else if (pid > 0) {
            printf("Parent created child %d with PID: %d\n", i+1, pid);
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    printf("Waiting for all children to complete...\n");
    for (int i = 0; i < 3; i++) {
        wait(NULL);
    }
    
    printf("All processes completed. Check dmesg for interception events!\n");
    printf("Run: dmesg | grep syscall_intercept\n");
    
    return 0;
}
