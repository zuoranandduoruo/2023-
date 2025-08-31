#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Experiment 2: Kernel Data Structures and List Management");
MODULE_VERSION("1.0");

#define PROC_NAME "kernel_list_demo"
#define MAX_RECORDS 20

struct demo_record {
    int id;
    char name[32];
    unsigned long timestamp;
    struct list_head list;
};

static struct proc_dir_entry *proc_entry;
static LIST_HEAD(record_list);
static int record_count = 0;
static int next_id = 1;

static void add_record(const char *name)
{
    struct demo_record *record;
    
    if (record_count >= MAX_RECORDS) {
        struct demo_record *oldest;
        oldest = list_first_entry(&record_list, struct demo_record, list);
        list_del(&oldest->list);
        printk(KERN_INFO "kernel_list: Removed oldest record ID=%d, Name=%s\n", 
               oldest->id, oldest->name);
        kfree(oldest);
        record_count--;
    }
    
    record = kmalloc(sizeof(struct demo_record), GFP_KERNEL);
    if (!record) {
        printk(KERN_ERR "kernel_list: Failed to allocate memory\n");
        return;
    }
    
    record->id = next_id++;
    strncpy(record->name, name, sizeof(record->name) - 1);
    record->name[sizeof(record->name) - 1] = '\0';
    record->timestamp = jiffies;
    
    list_add_tail(&record->list, &record_list);
    record_count++;
    
    printk(KERN_INFO "kernel_list: Added record ID=%d, Name=%s, Count=%d\n", 
           record->id, record->name, record_count);
}

static void remove_record_by_id(int id)
{
    struct demo_record *record, *tmp;
    
    list_for_each_entry_safe(record, tmp, &record_list, list) {
        if (record->id == id) {
            list_del(&record->list);
            printk(KERN_INFO "kernel_list: Removed record ID=%d, Name=%s\n", 
                   record->id, record->name);
            kfree(record);
            record_count--;
            return;
        }
    }
    
    printk(KERN_WARNING "kernel_list: Record with ID=%d not found\n", id);
}

static void clear_all_records(void)
{
    struct demo_record *record, *tmp;
    int cleared = 0;
    
    list_for_each_entry_safe(record, tmp, &record_list, list) {
        list_del(&record->list);
        kfree(record);
        cleared++;
    }
    
    record_count = 0;
    printk(KERN_INFO "kernel_list: Cleared %d records\n", cleared);
}

static int kernel_list_show(struct seq_file *m, void *v)
{
    struct demo_record *record;
    
    seq_printf(m, "=== Kernel List Management Demo ===\n");
    seq_printf(m, "Total Records: %d/%d\n", record_count, MAX_RECORDS);
    seq_printf(m, "Next ID: %d\n", next_id);
    seq_printf(m, "\n=== Record List ===\n");
    seq_printf(m, "%-4s %-20s %-12s\n", "ID", "NAME", "TIMESTAMP");
    seq_printf(m, "----------------------------------------\n");
    
    if (list_empty(&record_list)) {
        seq_printf(m, "No records found.\n");
    } else {
        list_for_each_entry(record, &record_list, list) {
            seq_printf(m, "%-4d %-20s %-12lu\n", 
                       record->id, record->name, record->timestamp);
        }
    }
    
    seq_printf(m, "\n=== Usage ===\n");
    seq_printf(m, "Add record: echo 'add <name>' > /proc/%s\n", PROC_NAME);
    seq_printf(m, "Remove record: echo 'del <id>' > /proc/%s\n", PROC_NAME);
    seq_printf(m, "Clear all: echo 'clear' > /proc/%s\n", PROC_NAME);
    
    return 0;
}

static int kernel_list_open(struct inode *inode, struct file *file)
{
    return single_open(file, kernel_list_show, NULL);
}

static ssize_t kernel_list_write(struct file *file, const char __user *buffer,
                                size_t count, loff_t *pos)
{
    char cmd[64];
    char name[32];
    int id;
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (cmd[count-1] == '\n')
        cmd[count-1] = '\0';
    
    if (strncmp(cmd, "add ", 4) == 0) {
        strncpy(name, cmd + 4, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        add_record(name);
    } else if (strncmp(cmd, "del ", 4) == 0) {
        if (sscanf(cmd + 4, "%d", &id) == 1) {
            remove_record_by_id(id);
        } else {
            printk(KERN_WARNING "kernel_list: Invalid delete command format\n");
        }
    } else if (strcmp(cmd, "clear") == 0) {
        clear_all_records();
    } else {
        printk(KERN_WARNING "kernel_list: Unknown command: %s\n", cmd);
    }
    
    return count;
}

static const struct proc_ops kernel_list_fops = {
    .proc_open = kernel_list_open,
    .proc_read = seq_read,
    .proc_write = kernel_list_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init kernel_list_init(void)
{
    printk(KERN_INFO "kernel_list: Loading Kernel List Management Module\n");
    
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &kernel_list_fops);
    if (!proc_entry) {
        printk(KERN_ERR "kernel_list: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "kernel_list: Module loaded successfully\n");
    printk(KERN_INFO "kernel_list: Use 'cat /proc/%s' to view records\n", PROC_NAME);
    printk(KERN_INFO "kernel_list: Use 'echo \"add <name>\" > /proc/%s' to add records\n", PROC_NAME);
    
    add_record("initial_record");
    add_record("demo_entry");
    
    return 0;
}

static void __exit kernel_list_exit(void)
{
    printk(KERN_INFO "kernel_list: Unloading Kernel List Management Module\n");
    
    proc_remove(proc_entry);
    clear_all_records();
    
    printk(KERN_INFO "kernel_list: Module unloaded successfully\n");
}

module_init(kernel_list_init);
module_exit(kernel_list_exit);
