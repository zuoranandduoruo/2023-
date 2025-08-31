#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

#define NUM_PROCESSES 3
#define OPERATIONS_PER_PROCESS 10

void send_command(const char *cmd) {
    char full_cmd[256];
    snprintf(full_cmd, sizeof(full_cmd), "echo '%s' | sudo tee /proc/concurrency_demo > /dev/null", cmd);
    system(full_cmd);
    printf("Sent command: %s\n", cmd);
}

void read_status() {
    printf("\n=== Current Status ===\n");
    system("cat /proc/concurrency_demo");
    printf("\n");
}

void test_individual_locks() {
    printf("=== Testing Individual Lock Types ===\n");
    
    printf("Testing spinlock operations:\n");
    for (int i = 1; i <= 5; i++) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "spin %d", i * 10);
        send_command(cmd);
        usleep(100000);
    }
    
    printf("\nTesting mutex operations:\n");
    for (int i = 1; i <= 5; i++) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "mutex %d", i * 20);
        send_command(cmd);
        usleep(100000);
    }
    
    printf("\nTesting rwlock operations:\n");
    for (int i = 1; i <= 5; i++) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "rwlock %d", i * 30);
        send_command(cmd);
        usleep(100000);
    }
    
    printf("\nTesting read operations:\n");
    for (int i = 0; i < 3; i++) {
        send_command("read");
        usleep(100000);
    }
    
    read_status();
}

void test_worker_threads() {
    printf("=== Testing Worker Threads ===\n");
    
    printf("Starting worker threads...\n");
    send_command("start");
    sleep(1);
    read_status();
    
    printf("Letting threads run for 5 seconds...\n");
    for (int i = 1; i <= 5; i++) {
        printf("Time: %d/5 seconds\n", i);
        sleep(1);
    }
    
    read_status();
    
    printf("Stopping worker threads...\n");
    send_command("stop");
    sleep(1);
    read_status();
}

void concurrent_process_worker(int process_id) {
    printf("Process %d: Starting concurrent operations\n", process_id);
    
    for (int i = 0; i < OPERATIONS_PER_PROCESS; i++) {
        char cmd[32];
        int operation = (process_id * OPERATIONS_PER_PROCESS + i) % 4;
        int value = process_id * 100 + i;
        
        switch (operation) {
            case 0:
                snprintf(cmd, sizeof(cmd), "spin %d", value);
                break;
            case 1:
                snprintf(cmd, sizeof(cmd), "mutex %d", value);
                break;
            case 2:
                snprintf(cmd, sizeof(cmd), "rwlock %d", value);
                break;
            case 3:
                strcpy(cmd, "read");
                break;
        }
        
        send_command(cmd);
        usleep(50000 + (process_id * 10000));
    }
    
    printf("Process %d: Completed operations\n", process_id);
}

void test_concurrent_access() {
    printf("=== Testing Concurrent Access ===\n");
    
    pid_t pids[NUM_PROCESSES];
    
    printf("Starting %d concurrent processes...\n", NUM_PROCESSES);
    
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            concurrent_process_worker(i);
            exit(0);
        } else if (pids[i] < 0) {
            perror("fork failed");
            exit(1);
        }
    }
    
    for (int i = 0; i < NUM_PROCESSES; i++) {
        wait(NULL);
    }
    
    printf("All concurrent processes completed\n");
    read_status();
}

void test_stress() {
    printf("=== Stress Test ===\n");
    
    send_command("clear");
    read_status();
    
    printf("Starting worker threads for stress test...\n");
    send_command("start");
    
    printf("Adding concurrent load from test program...\n");
    for (int round = 0; round < 3; round++) {
        printf("Stress round %d/3\n", round + 1);
        
        for (int i = 0; i < 20; i++) {
            char cmd[32];
            int op = i % 3;
            switch (op) {
                case 0:
                    snprintf(cmd, sizeof(cmd), "spin %d", 1000 + i);
                    break;
                case 1:
                    snprintf(cmd, sizeof(cmd), "mutex %d", 2000 + i);
                    break;
                case 2:
                    snprintf(cmd, sizeof(cmd), "rwlock %d", 3000 + i);
                    break;
            }
            send_command(cmd);
            usleep(10000);
        }
        
        sleep(2);
        read_status();
    }
    
    printf("Stopping worker threads...\n");
    send_command("stop");
    read_status();
}

int main(int argc, char *argv[]) {
    printf("=== Concurrency Control Test Program ===\n");
    printf("This program tests various locking mechanisms\n\n");
    
    if (argc > 1 && strcmp(argv[1], "auto") == 0) {
        printf("Running automated test sequence...\n\n");
        
        read_status();
        
        test_individual_locks();
        
        send_command("clear");
        
        test_worker_threads();
        
        send_command("clear");
        
        test_concurrent_access();
        
        send_command("clear");
        
        test_stress();
        
        send_command("clear");
        
        printf("Automated test completed!\n");
        return 0;
    }
    
    int choice;
    char input[64];
    
    while (1) {
        printf("\n=== Concurrency Control Test Menu ===\n");
        printf("1. Show current status\n");
        printf("2. Test individual locks\n");
        printf("3. Test worker threads\n");
        printf("4. Test concurrent access\n");
        printf("5. Run stress test\n");
        printf("6. Start worker threads\n");
        printf("7. Stop worker threads\n");
        printf("8. Clear all data\n");
        printf("9. Send custom command\n");
        printf("10. Exit\n");
        printf("Enter your choice (1-10): ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                read_status();
                break;
                
            case 2:
                test_individual_locks();
                break;
                
            case 3:
                test_worker_threads();
                break;
                
            case 4:
                test_concurrent_access();
                break;
                
            case 5:
                test_stress();
                break;
                
            case 6:
                send_command("start");
                break;
                
            case 7:
                send_command("stop");
                break;
                
            case 8:
                send_command("clear");
                break;
                
            case 9:
                printf("Enter command: ");
                if (scanf("%63s", input) == 1) {
                    send_command(input);
                }
                break;
                
            case 10:
                printf("Exiting...\n");
                return 0;
                
            default:
                printf("Invalid choice. Please select 1-10.\n");
                break;
        }
    }
    
    return 0;
}
