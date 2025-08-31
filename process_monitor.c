#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/kprobes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Process Monitor and Statistics Kernel Module");
MODULE_VERSION("1.0");

#define PROC_NAME "process_monitor"
#define MAX_PROCESS_RECORDS 1000

struct process_record {
    pid_t pid;
    pid_t ppid;
    char comm[TASK_COMM_LEN];
    unsigned long start_time;
    unsigned long end_time;
    int status;
    struct list_head list;
};

struct monitor_stats {
    unsigned long total_processes_created;
    unsigned long total_processes_exited;
    unsigned long current_processes;
    unsigned long peak_processes;
};

static struct proc_dir_entry *proc_entry;
static struct monitor_stats stats;
static LIST_HEAD(process_list);
static DEFINE_SPINLOCK(process_lock);
static int record_count = 0;

static struct kprobe kp_do_fork;
static struct kprobe kp_do_exit;

static void add_process_record(pid_t pid, pid_t ppid, const char *comm)
{
    struct process_record *record;
    unsigned long flags;
    
    record = kmalloc(sizeof(struct process_record), GFP_ATOMIC);
    if (!record) {
        printk(KERN_WARNING "process_monitor: Failed to allocate memory for process record\n");
        return;
    }
    
    record->pid = pid;
    record->ppid = ppid;
    strncpy(record->comm, comm, TASK_COMM_LEN);
    record->comm[TASK_COMM_LEN-1] = '\0';
    record->start_time = jiffies;
    record->end_time = 0;
    record->status = 1;
    
    spin_lock_irqsave(&process_lock, flags);
    
    if (record_count >= MAX_PROCESS_RECORDS) {
        struct process_record *oldest;
        oldest = list_first_entry(&process_list, struct process_record, list);
        list_del(&oldest->list);
        kfree(oldest);
        record_count--;
    }
    
    list_add_tail(&record->list, &process_list);
    record_count++;
    
    stats.total_processes_created++;
    stats.current_processes++;
    if (stats.current_processes > stats.peak_processes) {
        stats.peak_processes = stats.current_processes;
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    printk(KERN_INFO "process_monitor: Process created - PID: %d, PPID: %d, COMM: %s\n", 
           pid, ppid, comm);
}

static void mark_process_exit(pid_t pid)
{
    struct process_record *record;
    unsigned long flags;
    
    spin_lock_irqsave(&process_lock, flags);
    
    list_for_each_entry(record, &process_list, list) {
        if (record->pid == pid && record->status == 1) {
            record->end_time = jiffies;
            record->status = 0;
            stats.total_processes_exited++;
            stats.current_processes--;
            break;
        }
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    printk(KERN_INFO "process_monitor: Process exited - PID: %d\n", pid);
}

static int pre_handler_fork(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *task = current;
    add_process_record(task->pid, task->parent->pid, task->comm);
    return 0;
}

static int pre_handler_exit(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *task = current;
    mark_process_exit(task->pid);
    return 0;
}

static int process_monitor_show(struct seq_file *m, void *v)
{
    struct process_record *record;
    unsigned long flags;
    unsigned long uptime_jiffies;
    
    seq_printf(m, "=== Process Monitor Statistics ===\n");
    seq_printf(m, "Total Processes Created: %lu\n", stats.total_processes_created);
    seq_printf(m, "Total Processes Exited: %lu\n", stats.total_processes_exited);
    seq_printf(m, "Current Active Processes: %lu\n", stats.current_processes);
    seq_printf(m, "Peak Processes: %lu\n", stats.peak_processes);
    seq_printf(m, "Records in Memory: %d\n", record_count);
    seq_printf(m, "\n=== Recent Process Records ===\n");
    seq_printf(m, "%-8s %-8s %-16s %-12s %-12s %-8s\n", 
               "PID", "PPID", "COMMAND", "START_TIME", "END_TIME", "STATUS");
    seq_printf(m, "------------------------------------------------------------------------\n");
    
    spin_lock_irqsave(&process_lock, flags);
    
    list_for_each_entry(record, &process_list, list) {
        uptime_jiffies = record->end_time ? record->end_time : jiffies;
        seq_printf(m, "%-8d %-8d %-16s %-12lu %-12lu %-8s\n",
                   record->pid, record->ppid, record->comm,
                   record->start_time, record->end_time,
                   record->status ? "RUNNING" : "EXITED");
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    return 0;
}

static int process_monitor_open(struct inode *inode, struct file *file)
{
    return single_open(file, process_monitor_show, NULL);
}

static ssize_t process_monitor_write(struct file *file, const char __user *buffer,
                                   size_t count, loff_t *pos)
{
    char cmd[32];
    struct process_record *record, *tmp;
    unsigned long flags;
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    
    if (strncmp(cmd, "clear", 5) == 0) {
        spin_lock_irqsave(&process_lock, flags);
        
        list_for_each_entry_safe(record, tmp, &process_list, list) {
            list_del(&record->list);
            kfree(record);
        }
        
        memset(&stats, 0, sizeof(stats));
        record_count = 0;
        
        spin_unlock_irqrestore(&process_lock, flags);
        
        printk(KERN_INFO "process_monitor: Statistics cleared\n");
    }
    
    return count;
}

static const struct proc_ops process_monitor_fops = {
    .proc_open = process_monitor_open,
    .proc_read = seq_read,
    .proc_write = process_monitor_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init process_monitor_init(void)
{
    int ret;
    
    printk(KERN_INFO "process_monitor: Loading Process Monitor Module\n");
    
    memset(&stats, 0, sizeof(stats));
    
    proc_entry = proc_create(PROC_NAME, 0666, NULL, &process_monitor_fops);
    if (!proc_entry) {
        printk(KERN_ERR "process_monitor: Failed to create proc entry\n");
        return -ENOMEM;
    }
    
    kp_do_fork.symbol_name = "_do_fork";
    kp_do_fork.pre_handler = pre_handler_fork;
    ret = register_kprobe(&kp_do_fork);
    if (ret < 0) {
        printk(KERN_INFO "process_monitor: Trying alternative fork symbol\n");
        kp_do_fork.symbol_name = "kernel_clone";
        ret = register_kprobe(&kp_do_fork);
        if (ret < 0) {
            printk(KERN_ERR "process_monitor: Failed to register fork kprobe: %d\n", ret);
            proc_remove(proc_entry);
            return ret;
        }
    }
    
    kp_do_exit.symbol_name = "do_exit";
    kp_do_exit.pre_handler = pre_handler_exit;
    ret = register_kprobe(&kp_do_exit);
    if (ret < 0) {
        printk(KERN_ERR "process_monitor: Failed to register exit kprobe: %d\n", ret);
        unregister_kprobe(&kp_do_fork);
        proc_remove(proc_entry);
        return ret;
    }
    
    printk(KERN_INFO "process_monitor: Module loaded successfully\n");
    printk(KERN_INFO "process_monitor: Use 'cat /proc/%s' to view statistics\n", PROC_NAME);
    printk(KERN_INFO "process_monitor: Use 'echo clear > /proc/%s' to clear statistics\n", PROC_NAME);
    
    return 0;
}

static void __exit process_monitor_exit(void)
{
    struct process_record *record, *tmp;
    
    printk(KERN_INFO "process_monitor: Unloading Process Monitor Module\n");
    
    unregister_kprobe(&kp_do_fork);
    unregister_kprobe(&kp_do_exit);
    
    proc_remove(proc_entry);
    
    list_for_each_entry_safe(record, tmp, &process_list, list) {
        list_del(&record->list);
        kfree(record);
    }
    
    printk(KERN_INFO "process_monitor: Module unloaded successfully\n");
}

module_init(process_monitor_init);
module_exit(process_monitor_exit);
