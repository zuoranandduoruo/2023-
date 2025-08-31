# 通用Makefile - 根据需要修改obj-m行
# 实验1: obj-m += 01_syscall_intercept.o
# 实验2: obj-m += 02_kernel_list.o  
# 实验3: obj-m += 03_proc_interface.o
# 实验4: obj-m += 04_concurrency_control.o
# 实验5: obj-m += 05_complete_monitor.o

obj-m += 01_syscall_intercept.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean

.PHONY: all clean
