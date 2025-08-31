#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Experiment 1: System Call Interception with Kprobes");
MODULE_VERSION("1.0");

static struct kprobe kp_fork;
static struct kprobe kp_exit;
static unsigned long fork_count = 0;
static unsigned long exit_count = 0;

static int pre_handler_fork(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *task = current;
    fork_count++;
    
    printk(KERN_INFO "syscall_intercept: FORK detected - PID: %d, PPID: %d, COMM: %s, Total forks: %lu\n",
           task->pid, task->parent->pid, task->comm, fork_count);
    
    return 0;
}

static int pre_handler_exit(struct kprobe *p, struct pt_regs *regs)
{
    struct task_struct *task = current;
    exit_count++;
    
    printk(KERN_INFO "syscall_intercept: EXIT detected - PID: %d, COMM: %s, Total exits: %lu\n",
           task->pid, task->comm, exit_count);
    
    return 0;
}

static int __init syscall_intercept_init(void)
{
    int ret;
    
    printk(KERN_INFO "syscall_intercept: Loading System Call Intercept Module\n");
    printk(KERN_INFO "syscall_intercept: This module demonstrates kprobes usage\n");
    
    kp_fork.symbol_name = "_do_fork";
    kp_fork.pre_handler = pre_handler_fork;
    
    ret = register_kprobe(&kp_fork);
    if (ret < 0) {
        printk(KERN_INFO "syscall_intercept: Trying alternative fork symbol 'kernel_clone'\n");
        kp_fork.symbol_name = "kernel_clone";
        ret = register_kprobe(&kp_fork);
        if (ret < 0) {
            printk(KERN_ERR "syscall_intercept: Failed to register fork kprobe: %d\n", ret);
            return ret;
        }
    }
    printk(KERN_INFO "syscall_intercept: Fork kprobe registered on symbol: %s\n", kp_fork.symbol_name);
    
    kp_exit.symbol_name = "do_exit";
    kp_exit.pre_handler = pre_handler_exit;
    
    ret = register_kprobe(&kp_exit);
    if (ret < 0) {
        printk(KERN_ERR "syscall_intercept: Failed to register exit kprobe: %d\n", ret);
        unregister_kprobe(&kp_fork);
        return ret;
    }
    printk(KERN_INFO "syscall_intercept: Exit kprobe registered successfully\n");
    
    printk(KERN_INFO "syscall_intercept: Module loaded! Watch dmesg for process events\n");
    printk(KERN_INFO "syscall_intercept: Use 'dmesg | tail -f' to see real-time events\n");
    
    return 0;
}

static void __exit syscall_intercept_exit(void)
{
    printk(KERN_INFO "syscall_intercept: Unloading System Call Intercept Module\n");
    
    unregister_kprobe(&kp_fork);
    unregister_kprobe(&kp_exit);
    
    printk(KERN_INFO "syscall_intercept: Final statistics:\n");
    printk(KERN_INFO "syscall_intercept: Total fork events: %lu\n", fork_count);
    printk(KERN_INFO "syscall_intercept: Total exit events: %lu\n", exit_count);
    printk(KERN_INFO "syscall_intercept: Module unloaded successfully\n");
}

module_init(syscall_intercept_init);
module_exit(syscall_intercept_exit);
