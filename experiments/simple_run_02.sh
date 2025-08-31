#!/bin/bash

#实验2执行脚本 - 内核数据结构与链表管理

echo "========== 实验2：内核数据结构与链表管理 ========="

# 1. 修改Makefile
echo "1. 配置编译..."
sed -i 's/obj-m += .*/obj-m += 02_kernel_list.o/' Makefile

# 2. 编译
echo "2. 编译模块..."
make all

# 3. 编译测试程序
echo "3. 编译测试程序..."
gcc -o 02_test 02_test.c

# 4. 加载模块
echo "4. 加载模块..."
sudo insmod 02_kernel_list.ko

echo "5. 模块已加载，查看初始状态..."
cat /proc/kernel_list_demo

echo "6. 运行测试程序..."
./02_test auto

echo "7. 查看最终状态..."
cat /proc/kernel_list_demo

echo "8. 卸载模块..."
sudo rmmod 02_kernel_list

echo "9. 清理文件..."
make clean
rm -f 02_test

echo "=== 实验2完成！ ==="
