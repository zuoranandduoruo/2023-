#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwlock.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/random.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Experiment 4: Concurrency Control and Locking Mechanisms");
MODULE_VERSION("1.0");

#define PROC_NAME "concurrency_demo"
#define MAX_WORKERS 5
#define MAX_DATA_ITEMS 100

struct shared_data {
    int value;
    unsigned long access_count;
    struct list_head list;
};

struct lock_stats {
    unsigned long spinlock_ops;
    unsigned long mutex_ops;
    unsigned long rwlock_read_ops;
    unsigned long rwlock_write_ops;
    unsigned long contention_count;
};

static struct proc_dir_entry *proc_entry;
static LIST_HEAD(data_list);
static int data_count = 0;

static DEFINE_SPINLOCK(data_spinlock);
static DEFINE_MUTEX(data_mutex);
static DEFINE_RWLOCK(data_rwlock);

static struct lock_stats stats = {0};
static struct task_struct *worker_threads[MAX_WORKERS];
static int worker_running = 0;

static int next_value = 1;

static void spinlock_add_data(int value)
{
    struct shared_data *new_data;
    unsigned long flags;
    
    new_data = kmalloc(sizeof(struct shared_data), GFP_KERNEL);
    if (!new_data) {
        printk(KERN_ERR "concurrency: Failed to allocate memory\n");
        return;
    }
    
    new_data->value = value;
    new_data->access_count = 0;
    
    spin_lock_irqsave(&data_spinlock, flags);
    
    if (data_count >= MAX_DATA_ITEMS) {
        struct shared_data *oldest;
        oldest = list_first_entry(&data_list, struct shared_data, list);
        list_del(&oldest->list);
        kfree(oldest);
        data_count--;
    }
    
    list_add_tail(&new_data->list, &data_list);
    data_count++;
    stats.spinlock_ops++;
    
    spin_unlock_irqrestore(&data_spinlock, flags);
    
    printk(KERN_INFO "concurrency: Added data %d using spinlock\n", value);
}

static void mutex_add_data(int value)
{
    struct shared_data *new_data;
    
    new_data = kmalloc(sizeof(struct shared_data), GFP_KERNEL);
    if (!new_data) {
        printk(KERN_ERR "concurrency: Failed to allocate memory\n");
        return;
    }
    
    new_data->value = value;
    new_data->access_count = 0;
    
    if (mutex_lock_interruptible(&data_mutex)) {
        kfree(new_data);
        return;
    }
    
    if (data_count >= MAX_DATA_ITEMS) {
        struct shared_data *oldest;
        oldest = list_first_entry(&data_list, struct shared_data, list);
        list_del(&oldest->list);
        kfree(oldest);
        data_count--;
    }
    
    list_add_tail(&new_data->list, &data_list);
    data_count++;
    stats.mutex_ops++;
    
    msleep(1);
    
    mutex_unlock(&data_mutex);
    
    printk(KERN_INFO "concurrency: Added data %d using mutex\n", value);
}

static int rwlock_read_data(void)
{
    struct shared_data *data;
    int count = 0;
    unsigned long flags;
    
    read_lock_irqsave(&data_rwlock, flags);
    
    list_for_each_entry(data, &data_list, list) {
        data->access_count++;
        count++;
    }
    
    stats.rwlock_read_ops++;
    
    read_unlock_irqrestore(&data_rwlock, flags);
    
    return count;
}

static void rwlock_write_data(int value)
{
    struct shared_data *new_data;
    unsigned long flags;
    
    new_data = kmalloc(sizeof(struct shared_data), GFP_KERNEL);
    if (!new_data) {
        printk(KERN_ERR "concurrency: Failed to allocate memory\n");
        return;
    }
    
    new_data->value = value;
    new_data->access_count = 0;
    
    write_lock_irqsave(&data_rwlock, flags);
    
    if (data_count >= MAX_DATA_ITEMS) {
        struct shared_data *oldest;
        oldest = list_first_entry(&data_list, struct shared_data, list);
        list_del(&oldest->list);
        kfree(oldest);
        data_count--;
    }
    
    list_add_tail(&new_data->list, &data_list);
    data_count++;
    stats.rwlock_write_ops++;
    
    write_unlock_irqrestore(&data_rwlock, flags);
    
    printk(KERN_INFO "concurrency: Added data %d using rwlock\n", value);
}

static int worker_thread_func(void *data)
{
    int worker_id = (int)(long)data;
    int operation;
    
    printk(KERN_INFO "concurrency: Worker thread %d started\n", worker_id);
    
    while (!kthread_should_stop()) {
        get_random_bytes(&operation, sizeof(operation));
        operation = operation % 4;
        
        switch (operation) {
            case 0:
                spinlock_add_data(next_value++);
                break;
            case 1:
                mutex_add_data(next_value++);
                break;
            case 2:
                rwlock_read_data();
                break;
            case 3:
                rwlock_write_data(next_value++);
                break;
        }
        
        msleep(100 + (worker_id * 50));
        
        if (kthread_should_stop())
            break;
    }
    
    printk(KERN_INFO "concurrency: Worker thread %d stopped\n", worker_id);
    return 0;
}

static void start_worker_threads(void)
{
    int i;
    
    if (worker_running) {
        printk(KERN_INFO "concurrency: Worker threads already running\n");
        return;
    }
    
    worker_running = 1;
    
    for (i = 0; i < MAX_WORKERS; i++) {
        worker_threads[i] = kthread_run(worker_thread_func, (void *)(long)i, 
                                       "concurrency_worker_%d", i);
        if (IS_ERR(worker_threads[i])) {
            printk(KERN_ERR "concurrency: Failed to create worker thread %d\n", i);
            worker_threads[i] = NULL;
        }
    }
    
    printk(KERN_INFO "concurrency: Started %d worker threads\n", MAX_WORKERS);
}

static void stop_worker_threads(void)
{
    int i;
    
    if (!worker_running) {
        printk(KERN_INFO "concurrency: Worker threads not running\n");
        return;
    }
    
    for (i = 0; i < MAX_WORKERS; i++) {
        if (worker_threads[i]) {
            kthread_stop(worker_threads[i]);
            worker_threads[i] = NULL;
        }
    }
    
    worker_running = 0;
    printk(KERN_INFO "concurrency: Stopped all worker threads\n");
}

static void clear_all_data(void)
{
    struct shared_data *data, *tmp;
    unsigned long flags;
    
    spin_lock_irqsave(&data_spinlock, flags);
    
    list_for_each_entry_safe(data, tmp, &data_list, list) {
        list_del(&data->list);
        kfree(data);
    }
    
    data_count = 0;
    memset(&stats, 0, sizeof(stats));
    
    spin_unlock_irqrestore(&data_spinlock, flags);
    
    printk(KERN_INFO "concurrency: Cleared all data and statistics\n");
}

static int concurrency_show(struct seq_file *m, void *v)
{
    struct shared_data *data;
    unsigned long flags;
    
    seq_printf(m, "=== Concurrency Control Demo ===\n");
    seq_printf(m, "Data items: %d/%d\n", data_count, MAX_DATA_ITEMS);
    seq_printf(m, "Worker threads: %s\n", worker_running ? "RUNNING" : "STOPPED");
    seq_printf(m, "\n=== Lock Statistics ===\n");
    seq_printf(m, "Spinlock operations: %lu\n", stats.spinlock_ops);
    seq_printf(m, "Mutex operations: %lu\n", stats.mutex_ops);
    seq_printf(m, "RWLock read operations: %lu\n", stats.rwlock_read_ops);
    seq_printf(m, "RWLock write operations: %lu\n", stats.rwlock_write_ops);
    seq_printf(m, "Contention count: %lu\n", stats.contention_count);
    
    seq_printf(m, "\n=== Data List (first 10 items) ===\n");
    seq_printf(m, "%-8s %-12s\n", "VALUE", "ACCESS_COUNT");
    seq_printf(m, "--------------------\n");
    
    spin_lock_irqsave(&data_spinlock, flags);
    
    if (list_empty(&data_list)) {
        seq_printf(m, "No data items\n");
    } else {
        int count = 0;
        list_for_each_entry(data, &data_list, list) {
            if (count >= 10) break;
            seq_printf(m, "%-8d %-12lu\n", data->value, data->access_count);
            count++;
        }
        if (data_count > 10) {
            seq_printf(m, "... and %d more items\n", data_count - 10);
        }
    }
    
    spin_unlock_irqrestore(&data_spinlock, flags);
    
    seq_printf(m, "\n=== Commands ===\n");
    seq_printf(m, "start     - Start worker threads\n");
    seq_printf(m, "stop      - Stop worker threads\n");
    seq_printf(m, "clear     - Clear all data\n");
    seq_printf(m, "spin N    - Add data using spinlock\n");
    seq_printf(m, "mutex N   - Add data using mutex\n");
    seq_printf(m, "rwlock N  - Add data using rwlock\n");
    seq_printf(m, "read      - Read data using rwlock\n");
    
    return 0;
}

static int concurrency_open(struct inode *inode, struct file *file)
{
    return single_open(file, concurrency_show, NULL);
}

static ssize_t concurrency_write(struct file *file, const char __user *buffer,
                                size_t count, loff_t *pos)
{
    char cmd[64];
    char op[16];
    int value;
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (cmd[count-1] == '\n')
        cmd[count-1] = '\0';
    
    if (strcmp(cmd, "start") == 0) {
        start_worker_threads();
    } else if (strcmp(cmd, "stop") == 0) {
        stop_worker_threads();
    } else if (strcmp(cmd, "clear") == 0) {
        clear_all_data();
    } else if (strcmp(cmd, "read") == 0) {
        int count = rwlock_read_data();
        printk(KERN_INFO "concurrency: Read %d data items\n", count);
    } else if (sscanf(cmd, "%15s %d", op, &value) == 2) {
        if (strcmp(op, "spin") == 0) {
            spinlock_add_data(value);
        } else if (strcmp(op, "mutex") == 0) {
            mutex_add_data(value);
        } else if (strcmp(op, "rwlock") == 0) {
            rwlock_write_data(value);
        } else {
            printk(KERN_WARNING "concurrency: Unknown operation: %s\n", op);
        }
    } else {
        printk(KERN_WARNING "concurrency: Unknown command: %s\n", cmd);
    }
    
    return count;
}

static const struct proc_ops concurrency_fops = {
    .proc_open = concurrency_open,
    .proc_read = seq_read,
    .proc_write = concurrency_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init concurrency_init(void)
{
    printk(KERN_INFO "concurrency: Loading Concurrency Control Module\n");
    
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &concurrency_fops);
    if (!proc_entry) {
        printk(KERN_ERR "concurrency: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    printk(KERN_INFO "concurrency: Module loaded successfully\n");
    printk(KERN_INFO "concurrency: Use 'cat /proc/%s' to view status\n", PROC_NAME);
    printk(KERN_INFO "concurrency: Use 'echo start > /proc/%s' to start worker threads\n", PROC_NAME);
    
    return 0;
}

static void __exit concurrency_exit(void)
{
    printk(KERN_INFO "concurrency: Unloading Concurrency Control Module\n");
    
    stop_worker_threads();
    proc_remove(proc_entry);
    clear_all_data();
    
    printk(KERN_INFO "concurrency: Module unloaded successfully\n");
}

module_init(concurrency_init);
module_exit(concurrency_exit);
