#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>

void create_test_processes(int count) {
    pid_t pid;
    int i;
    
    printf("Creating %d test processes...\n", count);
    
    for (i = 0; i < count; i++) {
        pid = fork();
        
        if (pid == 0) {
            printf("Child process %d (PID: %d) started\n", i+1, getpid());
            sleep(2 + (i % 3));
            printf("Child process %d (PID: %d) exiting\n", i+1, getpid());
            exit(0);
        } else if (pid > 0) {
            printf("Parent created child %d with PID: %d\n", i+1, pid);
            usleep(100000);
        } else {
            perror("fork failed");
            exit(1);
        }
    }
    
    printf("Waiting for all children to complete...\n");
    for (i = 0; i < count; i++) {
        wait(NULL);
    }
    printf("All test processes completed.\n");
}

void stress_test() {
    printf("Running stress test with rapid process creation/destruction...\n");
    
    for (int round = 0; round < 3; round++) {
        printf("Stress test round %d/3\n", round + 1);
        
        for (int i = 0; i < 10; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                usleep(50000 + (i * 10000));
                exit(0);
            } else if (pid > 0) {
                usleep(10000);
            }
        }
        
        for (int i = 0; i < 10; i++) {
            wait(NULL);
        }
        
        sleep(1);
    }
    
    printf("Stress test completed.\n");
}

void display_monitor_stats() {
    printf("\n=== Current Process Monitor Statistics ===\n");
    system("cat /proc/process_monitor");
    printf("\n");
}

void clear_monitor_stats() {
    printf("Clearing process monitor statistics...\n");
    system("echo 'clear' | sudo tee /proc/process_monitor > /dev/null");
    printf("Statistics cleared.\n");
}

int main(int argc, char *argv[]) {
    int choice;
    int process_count;
    
    printf("=== Linux Kernel Process Monitor Test Program ===\n");
    printf("This program tests the process_monitor kernel module.\n\n");
    
    if (argc > 1) {
        if (strcmp(argv[1], "auto") == 0) {
            printf("Running automated test sequence...\n\n");
            
            clear_monitor_stats();
            sleep(1);
            
            printf("1. Creating 5 test processes...\n");
            create_test_processes(5);
            sleep(2);
            display_monitor_stats();
            
            printf("2. Running stress test...\n");
            stress_test();
            sleep(2);
            display_monitor_stats();
            
            printf("Automated test completed.\n");
            return 0;
        }
    }
    
    while (1) {
        printf("\nSelect an option:\n");
        printf("1. Create test processes\n");
        printf("2. Run stress test\n");
        printf("3. Display monitor statistics\n");
        printf("4. Clear monitor statistics\n");
        printf("5. Exit\n");
        printf("Enter your choice (1-5): ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                printf("Enter number of processes to create (1-20): ");
                if (scanf("%d", &process_count) != 1 || process_count < 1 || process_count > 20) {
                    printf("Invalid number. Using default value of 5.\n");
                    process_count = 5;
                }
                create_test_processes(process_count);
                break;
                
            case 2:
                stress_test();
                break;
                
            case 3:
                display_monitor_stats();
                break;
                
            case 4:
                clear_monitor_stats();
                break;
                
            case 5:
                printf("Exiting test program.\n");
                return 0;
                
            default:
                printf("Invalid choice. Please select 1-5.\n");
                break;
        }
    }
    
    return 0;
}
