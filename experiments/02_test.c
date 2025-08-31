#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void display_list() {
    printf("\n=== Current List State ===\n");
    system("cat /proc/kernel_list_demo");
    printf("\n");
}

void add_record(const char *name) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo 'add %s' | sudo tee /proc/kernel_list_demo > /dev/null", name);
    system(cmd);
    printf("Added record: %s\n", name);
}

void remove_record(int id) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo 'del %d' | sudo tee /proc/kernel_list_demo > /dev/null", id);
    system(cmd);
    printf("Removed record ID: %d\n", id);
}

void clear_all() {
    system("echo 'clear' | sudo tee /proc/kernel_list_demo > /dev/null");
    printf("Cleared all records\n");
}

int main(int argc, char *argv[]) {
    int choice;
    char name[32];
    int id;
    
    printf("=== Kernel List Management Test Program ===\n");
    printf("This program tests the kernel linked list functionality\n");
    
    if (argc > 1 && strcmp(argv[1], "auto") == 0) {
        printf("\nRunning automated test sequence...\n");
        
        display_list();
        
        printf("Adding test records...\n");
        add_record("student_1");
        add_record("student_2");
        add_record("student_3");
        add_record("teacher");
        display_list();
        
        printf("Removing record ID 2...\n");
        remove_record(2);
        display_list();
        
        printf("Adding more records to test LRU...\n");
        for (int i = 5; i <= 25; i++) {
            char temp_name[32];
            snprintf(temp_name, sizeof(temp_name), "record_%d", i);
            add_record(temp_name);
        }
        display_list();
        
        printf("Clearing all records...\n");
        clear_all();
        display_list();
        
        printf("Automated test completed!\n");
        return 0;
    }
    
    while (1) {
        printf("\n=== Kernel List Management Menu ===\n");
        printf("1. Display current list\n");
        printf("2. Add record\n");
        printf("3. Remove record by ID\n");
        printf("4. Clear all records\n");
        printf("5. Exit\n");
        printf("Enter your choice (1-5): ");
        
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Please enter a number.\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                display_list();
                break;
                
            case 2:
                printf("Enter record name: ");
                if (scanf("%31s", name) == 1) {
                    add_record(name);
                }
                break;
                
            case 3:
                printf("Enter record ID to remove: ");
                if (scanf("%d", &id) == 1) {
                    remove_record(id);
                }
                break;
                
            case 4:
                clear_all();
                break;
                
            case 5:
                printf("Exiting...\n");
                return 0;
                
            default:
                printf("Invalid choice. Please select 1-5.\n");
                break;
        }
    }
    
    return 0;
}
