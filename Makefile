obj-m += process_monitor.o

KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod process_monitor.ko

uninstall:
	sudo rmmod process_monitor

test:
	cat /proc/process_monitor

clear:
	echo "clear" | sudo tee /proc/process_monitor

load: all install

reload: uninstall clean all install

help:
	@echo "Available targets:"
	@echo "  all       - Build the kernel module"
	@echo "  clean     - Clean build files"
	@echo "  install   - Load the module into kernel"
	@echo "  uninstall - Remove the module from kernel"
	@echo "  test      - Display process monitor statistics"
	@echo "  clear     - Clear statistics"
	@echo "  load      - Build and install in one step"
	@echo "  reload    - Uninstall, clean, build and install"
	@echo "  help      - Show this help message"

.PHONY: all clean install uninstall test clear load reload help
