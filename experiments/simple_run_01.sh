#!/bin/bash

# 实验1执行脚本 

echo "========== 实验1：系统调用拦截 ========="

# 1. 编译
echo "1. 编译模块..."
make all

# 2. 编译测试程序
echo "2. 编译测试程序..."
gcc -o 01_test 01_test.c

# 3. 加载模块
echo "3. 加载模块..."
sudo insmod 01_syscall_intercept.ko

echo "4. 模块已加载，现在执行系统调用拦截..."

# 4. 运行测试
./01_test

echo "5. 查看拦截到的系统调用，请检查文字显示："
dmesg | grep syscall_intercept | tail -10

echo "6. 卸载模块..."
sudo rmmod 01_syscall_intercept

echo "7. 清理文件..."
make clean
rm -f 01_test

echo "=== 实验完成！ ==="
