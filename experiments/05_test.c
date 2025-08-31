#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>

#define PROC_BASE "/proc/process_monitor_complete"
#define MAX_PROCESSES 10

void send_command(const char *interface, const char *cmd) {
    char full_cmd[512];
    snprintf(full_cmd, sizeof(full_cmd), "echo '%s' | sudo tee %s/%s > /dev/null", 
             cmd, PROC_BASE, interface);
    system(full_cmd);
    printf("Sent to %s: %s\n", interface, cmd);
}

void read_interface(const char *interface) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", PROC_BASE, interface);
    printf("\n=== %s ===\n", interface);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cat %s", path);
    system(cmd);
    printf("\n");
}

void show_all_interfaces() {
    printf("=== Complete Process Monitor Status ===\n");
    read_interface("stats");
    read_interface("processes");
    read_interface("filter");
    read_interface("control");
}

void test_basic_functionality() {
    printf("=== Testing Basic Functionality ===\n");
    
    printf("1. Initial status:\n");
    read_interface("stats");
    
    printf("2. Creating test processes:\n");
    for (int i = 1; i <= 5; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            printf("Child process %d (PID: %d) running\n", i, getpid());
            sleep(2);
            printf("Child process %d (PID: %d) exiting\n", i, getpid());
            exit(0);
        } else if (pid > 0) {
            printf("Created child %d with PID: %d\n", i, pid);
        }
    }
    
    printf("3. Waiting for processes to complete...\n");
    for (int i = 0; i < 5; i++) {
        wait(NULL);
    }
    
    printf("4. Updated statistics:\n");
    read_interface("stats");
    
    printf("5. Process records:\n");
    read_interface("processes");
}

void test_filtering() {
    printf("=== Testing Process Filtering ===\n");
    
    printf("1. Current filter configuration:\n");
    read_interface("filter");
    
    printf("2. Setting filter for 'sleep' processes:\n");
    send_command("filter", "comm sleep");
    send_command("filter", "enable");
    read_interface("filter");
    
    printf("3. Creating mixed processes:\n");
    system("sleep 1 &");
    system("echo 'test' > /dev/null &");
    system("ls > /dev/null &");
    sleep(2);
    
    printf("4. Filtered process list (should show only sleep):\n");
    read_interface("processes");
    
    printf("5. Testing PID filter:\n");
    send_command("filter", "reset");
    char pid_cmd[64];
    snprintf(pid_cmd, sizeof(pid_cmd), "pid %d", getpid());
    send_command("filter", pid_cmd);
    send_command("filter", "enable");
    read_interface("filter");
    
    printf("6. Testing lifetime filter:\n");
    send_command("filter", "reset");
    send_command("filter", "minlife 1");
    send_command("filter", "maxlife 10");
    send_command("filter", "enable");
    read_interface("filter");
    
    printf("7. Resetting filter:\n");
    send_command("filter", "reset");
    read_interface("filter");
}

void test_control_commands() {
    printf("=== Testing Control Commands ===\n");
    
    printf("1. Current control status:\n");
    read_interface("control");
    
    printf("2. Stopping monitoring:\n");
    send_command("control", "stop");
    read_interface("stats");
    
    printf("3. Creating processes while monitoring is stopped:\n");
    system("sleep 0.5 &");
    system("echo 'test' > /dev/null &");
    sleep(1);
    
    printf("4. Statistics (should not change much):\n");
    read_interface("stats");
    
    printf("5. Restarting monitoring:\n");
    send_command("control", "start");
    read_interface("stats");
    
    printf("6. Creating processes with monitoring enabled:\n");
    for (int i = 0; i < 3; i++) {
        system("sleep 0.5 &");
    }
    sleep(2);
    
    printf("7. Updated statistics:\n");
    read_interface("stats");
    
    printf("8. Clearing all records:\n");
    send_command("control", "clear");
    read_interface("stats");
    
    printf("9. Resetting statistics:\n");
    send_command("control", "reset_stats");
    read_interface("stats");
}

void stress_test() {
    printf("=== Stress Test ===\n");
    
    printf("1. Clearing previous data:\n");
    send_command("control", "clear");
    send_command("control", "reset_stats");
    
    printf("2. Running stress test (creating many processes):\n");
    for (int round = 1; round <= 3; round++) {
        printf("Stress round %d/3:\n", round);
        
        for (int i = 0; i < 20; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                usleep(50000 + (i * 5000));
                exit(0);
            } else if (pid < 0) {
                perror("fork failed");
            }
            usleep(10000);
        }
        
        printf("Waiting for round %d processes...\n", round);
        for (int i = 0; i < 20; i++) {
            wait(NULL);
        }
        
        printf("Statistics after round %d:\n", round);
        read_interface("stats");
        
        sleep(1);
    }
    
    printf("3. Final stress test statistics:\n");
    read_interface("stats");
}

void concurrent_access_test() {
    printf("=== Concurrent Access Test ===\n");
    
    printf("1. Testing concurrent reads:\n");
    for (int i = 0; i < 5; i++) {
        if (fork() == 0) {
            for (int j = 0; j < 3; j++) {
                read_interface("stats");
                usleep(100000);
            }
            exit(0);
        }
    }
    
    printf("2. Testing concurrent writes:\n");
    for (int i = 0; i < 3; i++) {
        if (fork() == 0) {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "comm test_%d", i);
            send_command("filter", cmd);
            usleep(200000);
            send_command("filter", "reset");
            exit(0);
        }
    }
    
    printf("3. Waiting for concurrent operations to complete...\n");
    for (int i = 0; i < 8; i++) {
        wait(NULL);
    }
    
    printf("4. Final state after concurrent access:\n");
    show_all_interfaces();
}

void performance_test() {
    printf("=== Performance Test ===\n");
    
    printf("1. Measuring process creation overhead:\n");
    struct timespec start, end;
    
    send_command("control", "clear");
    send_command("control", "reset_stats");
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < 100; i++) {
        if (fork() == 0) {
            exit(0);
        }
    }
    
    for (int i = 0; i < 100; i++) {
        wait(NULL);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + 
                    (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    
    printf("2. Performance results:\n");
    printf("Created 100 processes in %.3f seconds\n", elapsed);
    printf("Average time per process: %.3f ms\n", (elapsed * 1000) / 100);
    
    read_interface("stats");
}

int main(int argc, char *argv[]) {
    printf("=== Complete Process Monitor Test Program ===\n");
    printf("This program comprehensively tests the complete monitoring system\n\n");
    
    if (argc > 1 && strcmp(argv[1], "auto") == 0) {
        printf("Running automated comprehensive test...\n\n");
        
        show_all_interfaces();
        
        test_basic_functionality();
        
        test_filtering();
        
        test_control_commands();
        
        stress_test();
        
        concurrent_access_test();
        
        performance_test();
        
        printf("=== Final System State ===\n");
        show_all_interfaces();
        
        printf("Comprehensive test completed!\n");
        return 0;
    }
    
    int choice;
    
    while (1) {
        printf("\n=== Complete Process Monitor Test Menu ===\n");
        printf("1. Show all interfaces\n");
        printf("2. Test basic functionality\n");
        printf("3. Test process filtering\n");
        printf("4. Test control commands\n");
        printf("5. Run stress test\n");
        printf("6. Test concurrent access\n");
        printf("7. Performance test\n");
        printf("8. Read specific interface\n");
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
                show_all_interfaces();
                break;
                
            case 2:
                test_basic_functionality();
                break;
                
            case 3:
                test_filtering();
                break;
                
            case 4:
                test_control_commands();
                break;
                
            case 5:
                stress_test();
                break;
                
            case 6:
                concurrent_access_test();
                break;
                
            case 7:
                performance_test();
                break;
                
            case 8: {
                char interface[64];
                printf("Enter interface name (stats/processes/filter/control): ");
                if (scanf("%63s", interface) == 1) {
                    read_interface(interface);
                }
                break;
            }
            
            case 9: {
                char interface[64], command[256];
                printf("Enter interface name: ");
                if (scanf("%63s", interface) == 1) {
                    printf("Enter command: ");
                    getchar(); // consume newline
                    if (fgets(command, sizeof(command), stdin)) {
                        command[strcspn(command, "\n")] = 0; // remove newline
                        send_command(interface, command);
                    }
                }
                break;
            }
            
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
