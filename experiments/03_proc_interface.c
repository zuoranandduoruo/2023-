#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Experiment 3: Proc Filesystem Interface");
MODULE_VERSION("1.0");

#define PROC_DIR_NAME "proc_demo"
#define MAX_BUFFER_SIZE 1024

struct proc_data {
    char info_buffer[MAX_BUFFER_SIZE];
    int counter;
    unsigned long last_access;
    char user_message[256];
};

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_info;
static struct proc_dir_entry *proc_counter;
static struct proc_dir_entry *proc_message;
static struct proc_dir_entry *proc_time;

static struct proc_data pdata = {
    .info_buffer = "Proc filesystem demo initialized\n",
    .counter = 0,
    .last_access = 0,
    .user_message = "Hello from kernel!\n"
};

static int info_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Proc Interface Demo ===\n");
    seq_printf(m, "Module: %s\n", THIS_MODULE->name);
    seq_printf(m, "Version: %s\n", "1.0");
    seq_printf(m, "Description: Demonstrates proc filesystem usage\n");
    seq_printf(m, "\nAvailable interfaces:\n");
    seq_printf(m, "  /proc/%s/info     - This information (read-only)\n", PROC_DIR_NAME);
    seq_printf(m, "  /proc/%s/counter  - Access counter (read/write)\n", PROC_DIR_NAME);
    seq_printf(m, "  /proc/%s/message  - User message (read/write)\n", PROC_DIR_NAME);
    seq_printf(m, "  /proc/%s/time     - Current time (read-only)\n", PROC_DIR_NAME);
    seq_printf(m, "\nCurrent status:\n");
    seq_printf(m, "  Counter: %d\n", pdata.counter);
    seq_printf(m, "  Last access: %lu\n", pdata.last_access);
    seq_printf(m, "  Message: %s", pdata.user_message);
    
    return 0;
}

static int counter_show(struct seq_file *m, void *v)
{
    pdata.counter++;
    pdata.last_access = jiffies;
    
    seq_printf(m, "Access counter: %d\n", pdata.counter);
    seq_printf(m, "Last access time: %lu jiffies\n", pdata.last_access);
    
    printk(KERN_INFO "proc_demo: Counter accessed, value=%d\n", pdata.counter);
    
    return 0;
}

static int message_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%s", pdata.user_message);
    return 0;
}

static int time_show(struct seq_file *m, void *v)
{
    struct timespec64 ts;
    
    ktime_get_real_ts64(&ts);
    
    seq_printf(m, "Current kernel time:\n");
    seq_printf(m, "  Seconds since epoch: %lld\n", ts.tv_sec);
    seq_printf(m, "  Nanoseconds: %ld\n", ts.tv_nsec);
    seq_printf(m, "  Jiffies: %lu\n", jiffies);
    seq_printf(m, "  HZ: %d\n", HZ);
    
    return 0;
}

static int info_open(struct inode *inode, struct file *file)
{
    return single_open(file, info_show, NULL);
}

static int counter_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_show, NULL);
}

static int message_open(struct inode *inode, struct file *file)
{
    return single_open(file, message_show, NULL);
}

static int time_open(struct inode *inode, struct file *file)
{
    return single_open(file, time_show, NULL);
}

static ssize_t counter_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t *pos)
{
    char cmd[32];
    int new_value;
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (cmd[count-1] == '\n')
        cmd[count-1] = '\0';
    
    if (strcmp(cmd, "reset") == 0) {
        pdata.counter = 0;
        printk(KERN_INFO "proc_demo: Counter reset to 0\n");
    } else if (sscanf(cmd, "%d", &new_value) == 1) {
        pdata.counter = new_value;
        printk(KERN_INFO "proc_demo: Counter set to %d\n", new_value);
    } else {
        printk(KERN_WARNING "proc_demo: Invalid counter command: %s\n", cmd);
        return -EINVAL;
    }
    
    pdata.last_access = jiffies;
    return count;
}

static ssize_t message_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t *pos)
{
    if (count >= sizeof(pdata.user_message))
        return -EINVAL;
    
    if (copy_from_user(pdata.user_message, buffer, count))
        return -EFAULT;
    
    pdata.user_message[count] = '\0';
    
    printk(KERN_INFO "proc_demo: User message updated: %s", pdata.user_message);
    
    return count;
}

static const struct proc_ops info_fops = {
    .proc_open = info_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops counter_fops = {
    .proc_open = counter_open,
    .proc_read = seq_read,
    .proc_write = counter_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops message_fops = {
    .proc_open = message_open,
    .proc_read = seq_read,
    .proc_write = message_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops time_fops = {
    .proc_open = time_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init proc_interface_init(void)
{
    printk(KERN_INFO "proc_demo: Loading Proc Interface Module\n");
    
    proc_dir = proc_mkdir(PROC_DIR_NAME, NULL);
    if (!proc_dir) {
        printk(KERN_ERR "proc_demo: Failed to create proc directory\n");
        return -ENOMEM;
    }
    
    proc_info = proc_create("info", 0444, proc_dir, &info_fops);
    if (!proc_info) {
        printk(KERN_ERR "proc_demo: Failed to create info entry\n");
        goto cleanup_dir;
    }
    
    proc_counter = proc_create("counter", 0666, proc_dir, &counter_fops);
    if (!proc_counter) {
        printk(KERN_ERR "proc_demo: Failed to create counter entry\n");
        goto cleanup_info;
    }
    
    proc_message = proc_create("message", 0666, proc_dir, &message_fops);
    if (!proc_message) {
        printk(KERN_ERR "proc_demo: Failed to create message entry\n");
        goto cleanup_counter;
    }
    
    proc_time = proc_create("time", 0444, proc_dir, &time_fops);
    if (!proc_time) {
        printk(KERN_ERR "proc_demo: Failed to create time entry\n");
        goto cleanup_message;
    }
    
    printk(KERN_INFO "proc_demo: Module loaded successfully\n");
    printk(KERN_INFO "proc_demo: Proc directory created at /proc/%s/\n", PROC_DIR_NAME);
    printk(KERN_INFO "proc_demo: Available files: info, counter, message, time\n");
    
    return 0;

cleanup_message:
    proc_remove(proc_message);
cleanup_counter:
    proc_remove(proc_counter);
cleanup_info:
    proc_remove(proc_info);
cleanup_dir:
    proc_remove(proc_dir);
    return -ENOMEM;
}

static void __exit proc_interface_exit(void)
{
    printk(KERN_INFO "proc_demo: Unloading Proc Interface Module\n");
    
    proc_remove(proc_time);
    proc_remove(proc_message);
    proc_remove(proc_counter);
    proc_remove(proc_info);
    proc_remove(proc_dir);
    
    printk(KERN_INFO "proc_demo: All proc entries removed\n");
    printk(KERN_INFO "proc_demo: Final counter value: %d\n", pdata.counter);
    printk(KERN_INFO "proc_demo: Module unloaded successfully\n");
}

module_init(proc_interface_init);
module_exit(proc_interface_exit);
