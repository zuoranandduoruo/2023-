#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void read_file(const char *path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cat %s", path);
    printf("=== Reading %s ===\n", path);
    system(cmd);
    printf("\n");
}

void write_file(const char *path, const char *content) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "echo '%s' | sudo tee %s > /dev/null", content, path);
    system(cmd);
    printf("Wrote '%s' to %s\n", content, path);
}

void test_counter() {
    const char *counter_path = "/proc/proc_demo/counter";
    
    printf("=== Testing Counter Interface ===\n");
    
    printf("Reading counter multiple times:\n");
    for (int i = 1; i <= 5; i++) {
        printf("Read %d: ", i);
        system("cat /proc/proc_demo/counter | head -1");
    }
    printf("\n");
    
    printf("Setting counter to 50:\n");
    write_file(counter_path, "50");
    read_file(counter_path);
    
    printf("Setting counter to 100:\n");
    write_file(counter_path, "100");
    read_file(counter_path);
    
    printf("Resetting counter:\n");
    write_file(counter_path, "reset");
    read_file(counter_path);
}

void test_message() {
    const char *message_path = "/proc/proc_demo/message";
    
    printf("=== Testing Message Interface ===\n");
    
    printf("Initial message:\n");
    read_file(message_path);
    
    printf("Setting custom message:\n");
    write_file(message_path, "Hello from test program!");
    read_file(message_path);
    
    printf("Setting multiline message:\n");
    write_file(message_path, "Line 1\\nLine 2\\nLine 3");
    read_file(message_path);
    
    printf("Setting long message:\n");
    write_file(message_path, "This is a very long message to test the buffer limits of the proc interface");
    read_file(message_path);
}

void test_time() {
    printf("=== Testing Time Interface ===\n");
    
    printf("Reading time multiple times:\n");
    for (int i = 1; i <= 3; i++) {
        printf("Time read %d:\n", i);
        read_file("/proc/proc_demo/time");
        sleep(1);
    }
}

void show_directory() {
    printf("=== Proc Directory Contents ===\n");
    system("ls -la /proc/proc_demo/");
    printf("\n");
}

int main(int argc, char *argv[]) {
    printf("=== Proc Filesystem Interface Test Program ===\n");
    printf("This program tests various proc filesystem interfaces\n\n");
    
    if (argc > 1 && strcmp(argv[1], "auto") == 0) {
        printf("Running automated test sequence...\n\n");
        
        show_directory();
        
        read_file("/proc/proc_demo/info");
        
        test_counter();
        
        test_message();
        
        test_time();
        
        printf("=== Final Info Check ===\n");
        read_file("/proc/proc_demo/info");
        
        printf("Automated test completed!\n");
        return 0;
    }
    
    int choice;
    char input[256];
    
    while (1) {
        printf("\n=== Proc Interface Test Menu ===\n");
        printf("1. Show directory contents\n");
        printf("2. Read info\n");
        printf("3. Test counter interface\n");
        printf("4. Test message interface\n");
        printf("5. Read time\n");
        printf("6. Custom counter value\n");
        printf("7. Custom message\n");
        printf("8. Exit\n");
        printf("Enter your choice (1-8): ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                show_directory();
                break;
                
            case 2:
                read_file("/proc/proc_demo/info");
                break;
                
            case 3:
                test_counter();
                break;
                
            case 4:
                test_message();
                break;
                
            case 5:
                read_file("/proc/proc_demo/time");
                break;
                
            case 6:
                printf("Enter counter value (or 'reset'): ");
                if (scanf("%255s", input) == 1) {
                    write_file("/proc/proc_demo/counter", input);
                    read_file("/proc/proc_demo/counter");
                }
                break;
                
            case 7:
                printf("Enter message: ");
                getchar(); // consume newline
                if (fgets(input, sizeof(input), stdin)) {
                    // Remove trailing newline
                    input[strcspn(input, "\n")] = 0;
                    write_file("/proc/proc_demo/message", input);
                    read_file("/proc/proc_demo/message");
                }
                break;
                
            case 8:
                printf("Exiting...\n");
                return 0;
                
            default:
                printf("Invalid choice. Please select 1-8.\n");
                break;
        }
    }
    
    return 0;
}
