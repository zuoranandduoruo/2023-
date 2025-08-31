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
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/hashtable.h>
#include <linux/rbtree.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Experiment 5: Complete Process Monitoring System");
MODULE_VERSION("1.0");

#define PROC_DIR_NAME "process_monitor_complete"
#define MAX_PROCESS_RECORDS 1000
#define PROCESS_HASH_BITS 8

struct process_record {
    pid_t pid;
    pid_t ppid;
    char comm[TASK_COMM_LEN];
    unsigned long start_time;
    unsigned long end_time;
    int status;
    unsigned long cpu_time;
    unsigned long memory_usage;
    int exit_code;
    struct list_head list;
    struct hlist_node hash;
    struct rb_node rb_node;
};

struct monitor_stats {
    unsigned long total_processes_created;
    unsigned long total_processes_exited;
    unsigned long current_processes;
    unsigned long peak_processes;
    unsigned long total_cpu_time;
    unsigned long avg_lifetime;
    unsigned long longest_lifetime;
    unsigned long shortest_lifetime;
};

struct filter_config {
    pid_t target_pid;
    pid_t target_ppid;
    char target_comm[TASK_COMM_LEN];
    int min_lifetime;
    int max_lifetime;
    int enabled;
};

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_stats;
static struct proc_dir_entry *proc_processes;
static struct proc_dir_entry *proc_filter;
static struct proc_dir_entry *proc_control;

static struct monitor_stats stats;
static struct filter_config filter = {0};
static LIST_HEAD(process_list);
static DEFINE_HASHTABLE(process_hash, PROCESS_HASH_BITS);
static struct rb_root process_tree = RB_ROOT;
static DEFINE_SPINLOCK(process_lock);
static DEFINE_MUTEX(config_mutex);
static int record_count = 0;
static int monitoring_enabled = 1;

static struct kprobe kp_do_fork;
static struct kprobe kp_do_exit;

static struct process_record *find_process_by_pid(pid_t pid)
{
    struct process_record *record;
    
    hash_for_each_possible(process_hash, record, hash, pid) {
        if (record->pid == pid && record->status == 1) {
            return record;
        }
    }
    
    return NULL;
}

static void insert_process_rb_tree(struct process_record *record)
{
    struct rb_node **new = &(process_tree.rb_node);
    struct rb_node *parent = NULL;
    struct process_record *this;
    
    while (*new) {
        this = container_of(*new, struct process_record, rb_node);
        parent = *new;
        
        if (record->start_time < this->start_time)
            new = &((*new)->rb_left);
        else
            new = &((*new)->rb_right);
    }
    
    rb_link_node(&record->rb_node, parent, new);
    rb_insert_color(&record->rb_node, &process_tree);
}

static void remove_process_rb_tree(struct process_record *record)
{
    rb_erase(&record->rb_node, &process_tree);
}

static int process_matches_filter(struct process_record *record)
{
    if (!filter.enabled)
        return 1;
    
    if (filter.target_pid && record->pid != filter.target_pid)
        return 0;
    
    if (filter.target_ppid && record->ppid != filter.target_ppid)
        return 0;
    
    if (strlen(filter.target_comm) && 
        strncmp(record->comm, filter.target_comm, TASK_COMM_LEN) != 0)
        return 0;
    
    if (record->status == 0) {
        unsigned long lifetime = (record->end_time - record->start_time) / HZ;
        if (filter.min_lifetime && lifetime < filter.min_lifetime)
            return 0;
        if (filter.max_lifetime && lifetime > filter.max_lifetime)
            return 0;
    }
    
    return 1;
}

static void add_process_record(pid_t pid, pid_t ppid, const char *comm)
{
    struct process_record *record;
    unsigned long flags;
    
    if (!monitoring_enabled)
        return;
    
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
    record->cpu_time = 0;
    record->memory_usage = 0;
    record->exit_code = 0;
    
    spin_lock_irqsave(&process_lock, flags);
    
    if (record_count >= MAX_PROCESS_RECORDS) {
        struct process_record *oldest;
        oldest = list_first_entry(&process_list, struct process_record, list);
        list_del(&oldest->list);
        hash_del(&oldest->hash);
        remove_process_rb_tree(oldest);
        kfree(oldest);
        record_count--;
    }
    
    list_add_tail(&record->list, &process_list);
    hash_add(process_hash, &record->hash, pid);
    insert_process_rb_tree(record);
    record_count++;
    
    stats.total_processes_created++;
    stats.current_processes++;
    if (stats.current_processes > stats.peak_processes) {
        stats.peak_processes = stats.current_processes;
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    if (process_matches_filter(record)) {
        printk(KERN_INFO "process_monitor: Process created - PID: %d, PPID: %d, COMM: %s\n", 
               pid, ppid, comm);
    }
}

static void mark_process_exit(pid_t pid, int exit_code)
{
    struct process_record *record;
    unsigned long flags;
    unsigned long lifetime;
    
    if (!monitoring_enabled)
        return;
    
    spin_lock_irqsave(&process_lock, flags);
    
    record = find_process_by_pid(pid);
    if (record) {
        record->end_time = jiffies;
        record->status = 0;
        record->exit_code = exit_code;
        
        lifetime = (record->end_time - record->start_time) / HZ;
        
        stats.total_processes_exited++;
        stats.current_processes--;
        stats.total_cpu_time += record->cpu_time;
        
        if (stats.total_processes_exited == 1) {
            stats.longest_lifetime = lifetime;
            stats.shortest_lifetime = lifetime;
        } else {
            if (lifetime > stats.longest_lifetime)
                stats.longest_lifetime = lifetime;
            if (lifetime < stats.shortest_lifetime)
                stats.shortest_lifetime = lifetime;
        }
        
        if (stats.total_processes_exited > 0) {
            stats.avg_lifetime = (stats.avg_lifetime * (stats.total_processes_exited - 1) + lifetime) / 
                               stats.total_processes_exited;
        }
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    if (record && process_matches_filter(record)) {
        printk(KERN_INFO "process_monitor: Process exited - PID: %d, Exit Code: %d, Lifetime: %lu sec\n", 
               pid, exit_code, lifetime);
    }
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
    mark_process_exit(task->pid, task->exit_code);
    return 0;
}

static int stats_show(struct seq_file *m, void *v)
{
    unsigned long flags;
    
    spin_lock_irqsave(&process_lock, flags);
    
    seq_printf(m, "=== Process Monitor Statistics ===\n");
    seq_printf(m, "Monitoring Status: %s\n", monitoring_enabled ? "ENABLED" : "DISABLED");
    seq_printf(m, "Total Processes Created: %lu\n", stats.total_processes_created);
    seq_printf(m, "Total Processes Exited: %lu\n", stats.total_processes_exited);
    seq_printf(m, "Current Active Processes: %lu\n", stats.current_processes);
    seq_printf(m, "Peak Processes: %lu\n", stats.peak_processes);
    seq_printf(m, "Records in Memory: %d/%d\n", record_count, MAX_PROCESS_RECORDS);
    seq_printf(m, "\n=== Performance Statistics ===\n");
    seq_printf(m, "Total CPU Time: %lu jiffies\n", stats.total_cpu_time);
    seq_printf(m, "Average Lifetime: %lu seconds\n", stats.avg_lifetime);
    seq_printf(m, "Longest Lifetime: %lu seconds\n", stats.longest_lifetime);
    seq_printf(m, "Shortest Lifetime: %lu seconds\n", stats.shortest_lifetime);
    
    if (stats.total_processes_exited > 0 && jiffies > 0) {
        unsigned long uptime_seconds = jiffies / HZ;
        if (uptime_seconds > 0) {
            seq_printf(m, "Process Turnover Rate: %lu proc/sec\n", 
                       stats.total_processes_exited / uptime_seconds);
        }
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    return 0;
}

static int processes_show(struct seq_file *m, void *v)
{
    struct process_record *record;
    unsigned long flags;
    int count = 0;
    
    seq_printf(m, "=== Process Records ===\n");
    seq_printf(m, "%-8s %-8s %-16s %-12s %-12s %-8s %-8s %-12s\n", 
               "PID", "PPID", "COMMAND", "START_TIME", "END_TIME", "STATUS", "EXIT_CODE", "LIFETIME");
    seq_printf(m, "--------------------------------------------------------------------------------\n");
    
    spin_lock_irqsave(&process_lock, flags);
    
    list_for_each_entry(record, &process_list, list) {
        unsigned long lifetime = 0;
        
        if (!process_matches_filter(record))
            continue;
            
        if (record->status == 0 && record->end_time > record->start_time) {
            lifetime = (record->end_time - record->start_time) / HZ;
        }
        
        seq_printf(m, "%-8d %-8d %-16s %-12lu %-12lu %-8s %-8d %-12lu\n",
                   record->pid, record->ppid, record->comm,
                   record->start_time, record->end_time,
                   record->status ? "RUNNING" : "EXITED",
                   record->exit_code, lifetime);
        
        count++;
        if (count >= 50) {
            seq_printf(m, "... (showing first 50 matching records)\n");
            break;
        }
    }
    
    spin_unlock_irqrestore(&process_lock, flags);
    
    seq_printf(m, "\nTotal matching records: %d\n", count);
    
    return 0;
}

static int filter_show(struct seq_file *m, void *v)
{
    mutex_lock(&config_mutex);
    
    seq_printf(m, "=== Process Filter Configuration ===\n");
    seq_printf(m, "Filter Enabled: %s\n", filter.enabled ? "YES" : "NO");
    seq_printf(m, "Target PID: %d (0 = any)\n", filter.target_pid);
    seq_printf(m, "Target PPID: %d (0 = any)\n", filter.target_ppid);
    seq_printf(m, "Target Command: %s (empty = any)\n", 
               strlen(filter.target_comm) ? filter.target_comm : "(any)");
    seq_printf(m, "Min Lifetime: %d seconds (0 = no limit)\n", filter.min_lifetime);
    seq_printf(m, "Max Lifetime: %d seconds (0 = no limit)\n", filter.max_lifetime);
    
    seq_printf(m, "\n=== Filter Commands ===\n");
    seq_printf(m, "enable           - Enable filtering\n");
    seq_printf(m, "disable          - Disable filtering\n");
    seq_printf(m, "pid <pid>        - Filter by PID\n");
    seq_printf(m, "ppid <ppid>      - Filter by parent PID\n");
    seq_printf(m, "comm <command>   - Filter by command name\n");
    seq_printf(m, "minlife <sec>    - Minimum lifetime filter\n");
    seq_printf(m, "maxlife <sec>    - Maximum lifetime filter\n");
    seq_printf(m, "reset            - Reset all filters\n");
    
    mutex_unlock(&config_mutex);
    
    return 0;
}

static int control_show(struct seq_file *m, void *v)
{
    seq_printf(m, "=== Process Monitor Control ===\n");
    seq_printf(m, "Monitoring: %s\n", monitoring_enabled ? "ENABLED" : "DISABLED");
    seq_printf(m, "Records: %d/%d\n", record_count, MAX_PROCESS_RECORDS);
    
    seq_printf(m, "\n=== Control Commands ===\n");
    seq_printf(m, "start            - Start monitoring\n");
    seq_printf(m, "stop             - Stop monitoring\n");
    seq_printf(m, "clear            - Clear all records\n");
    seq_printf(m, "reset_stats      - Reset statistics\n");
    
    return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stats_show, NULL);
}

static int processes_open(struct inode *inode, struct file *file)
{
    return single_open(file, processes_show, NULL);
}

static int filter_open(struct inode *inode, struct file *file)
{
    return single_open(file, filter_show, NULL);
}

static int control_open(struct inode *inode, struct file *file)
{
    return single_open(file, control_show, NULL);
}

static ssize_t filter_write(struct file *file, const char __user *buffer,
                           size_t count, loff_t *pos)
{
    char cmd[128];
    char op[32], value[64];
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    if (cmd[count-1] == '\n')
        cmd[count-1] = '\0';
    
    mutex_lock(&config_mutex);
    
    if (strcmp(cmd, "enable") == 0) {
        filter.enabled = 1;
    } else if (strcmp(cmd, "disable") == 0) {
        filter.enabled = 0;
    } else if (strcmp(cmd, "reset") == 0) {
        memset(&filter, 0, sizeof(filter));
    } else if (sscanf(cmd, "%31s %63s", op, value) == 2) {
        if (strcmp(op, "pid") == 0) {
            filter.target_pid = simple_strtol(value, NULL, 10);
        } else if (strcmp(op, "ppid") == 0) {
            filter.target_ppid = simple_strtol(value, NULL, 10);
        } else if (strcmp(op, "comm") == 0) {
            strncpy(filter.target_comm, value, TASK_COMM_LEN - 1);
            filter.target_comm[TASK_COMM_LEN - 1] = '\0';
        } else if (strcmp(op, "minlife") == 0) {
            filter.min_lifetime = simple_strtol(value, NULL, 10);
        } else if (strcmp(op, "maxlife") == 0) {
            filter.max_lifetime = simple_strtol(value, NULL, 10);
        }
    }
    
    mutex_unlock(&config_mutex);
    
    return count;
}

static ssize_t control_write(struct file *file, const char __user *buffer,
                            size_t count, loff_t *pos)
{
    char cmd[64];
    struct process_record *record, *tmp;
    unsigned long flags;
    
    if (count >= sizeof(cmd))
        return -EINVAL;
    
    if (copy_from_user(cmd, buffer, count))
        return -EFAULT;
    
    cmd[count] = '\0';
    if (cmd[count-1] == '\n')
        cmd[count-1] = '\0';
    
    if (strcmp(cmd, "start") == 0) {
        monitoring_enabled = 1;
        printk(KERN_INFO "process_monitor: Monitoring started\n");
    } else if (strcmp(cmd, "stop") == 0) {
        monitoring_enabled = 0;
        printk(KERN_INFO "process_monitor: Monitoring stopped\n");
    } else if (strcmp(cmd, "clear") == 0) {
        spin_lock_irqsave(&process_lock, flags);
        
        list_for_each_entry_safe(record, tmp, &process_list, list) {
            list_del(&record->list);
            hash_del(&record->hash);
            remove_process_rb_tree(record);
            kfree(record);
        }
        
        record_count = 0;
        
        spin_unlock_irqrestore(&process_lock, flags);
        
        printk(KERN_INFO "process_monitor: All records cleared\n");
    } else if (strcmp(cmd, "reset_stats") == 0) {
        spin_lock_irqsave(&process_lock, flags);
        memset(&stats, 0, sizeof(stats));
        spin_unlock_irqrestore(&process_lock, flags);
        
        printk(KERN_INFO "process_monitor: Statistics reset\n");
    }
    
    return count;
}

static const struct proc_ops stats_fops = {
    .proc_open = stats_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops processes_fops = {
    .proc_open = processes_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops filter_fops = {
    .proc_open = filter_open,
    .proc_read = seq_read,
    .proc_write = filter_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops control_fops = {
    .proc_open = control_open,
    .proc_read = seq_read,
    .proc_write = control_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init complete_monitor_init(void)
{
    int ret;
    
    printk(KERN_INFO "process_monitor: Loading Complete Process Monitor Module\n");
    
    memset(&stats, 0, sizeof(stats));
    
    proc_dir = proc_mkdir(PROC_DIR_NAME, NULL);
    if (!proc_dir) {
        printk(KERN_ERR "process_monitor: Failed to create proc directory\n");
        return -ENOMEM;
    }
    
    proc_stats = proc_create("stats", 0444, proc_dir, &stats_fops);
    proc_processes = proc_create("processes", 0444, proc_dir, &processes_fops);
    proc_filter = proc_create("filter", 0666, proc_dir, &filter_fops);
    proc_control = proc_create("control", 0666, proc_dir, &control_fops);
    
    if (!proc_stats || !proc_processes || !proc_filter || !proc_control) {
        printk(KERN_ERR "process_monitor: Failed to create proc entries\n");
        goto cleanup_proc;
    }
    
    kp_do_fork.symbol_name = "_do_fork";
    kp_do_fork.pre_handler = pre_handler_fork;
    ret = register_kprobe(&kp_do_fork);
    if (ret < 0) {
        kp_do_fork.symbol_name = "kernel_clone";
        ret = register_kprobe(&kp_do_fork);
        if (ret < 0) {
            printk(KERN_ERR "process_monitor: Failed to register fork kprobe: %d\n", ret);
            goto cleanup_proc;
        }
    }
    
    kp_do_exit.symbol_name = "do_exit";
    kp_do_exit.pre_handler = pre_handler_exit;
    ret = register_kprobe(&kp_do_exit);
    if (ret < 0) {
        printk(KERN_ERR "process_monitor: Failed to register exit kprobe: %d\n", ret);
        unregister_kprobe(&kp_do_fork);
        goto cleanup_proc;
    }
    
    printk(KERN_INFO "process_monitor: Module loaded successfully\n");
    printk(KERN_INFO "process_monitor: Proc directory: /proc/%s/\n", PROC_DIR_NAME);
    printk(KERN_INFO "process_monitor: Available interfaces: stats, processes, filter, control\n");
    
    return 0;

cleanup_proc:
    if (proc_control) proc_remove(proc_control);
    if (proc_filter) proc_remove(proc_filter);
    if (proc_processes) proc_remove(proc_processes);
    if (proc_stats) proc_remove(proc_stats);
    if (proc_dir) proc_remove(proc_dir);
    return -ENOMEM;
}

static void __exit complete_monitor_exit(void)
{
    struct process_record *record, *tmp;
    
    printk(KERN_INFO "process_monitor: Unloading Complete Process Monitor Module\n");
    
    unregister_kprobe(&kp_do_fork);
    unregister_kprobe(&kp_do_exit);
    
    proc_remove(proc_control);
    proc_remove(proc_filter);
    proc_remove(proc_processes);
    proc_remove(proc_stats);
    proc_remove(proc_dir);
    
    list_for_each_entry_safe(record, tmp, &process_list, list) {
        list_del(&record->list);
        kfree(record);
    }
    
    printk(KERN_INFO "process_monitor: Final statistics - Created: %lu, Exited: %lu\n",
           stats.total_processes_created, stats.total_processes_exited);
    printk(KERN_INFO "process_monitor: Module unloaded successfully\n");
}

module_init(complete_monitor_init);
module_exit(complete_monitor_exit);
