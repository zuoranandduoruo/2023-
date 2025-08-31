#!/bin/bash

# 实验3执行脚本 - proc文件系统接口

echo "========== 实验3：proc文件系统接口 ========="

# 1. 修改Makefile
echo "1. 配置编译..."
sed -i 's/obj-m += .*/obj-m += 03_proc_interface.o/' Makefile

# 2. 编译
echo "2. 编译模块..."
make all

# 3. 编译测试程序
echo "3. 编译测试程序..."
gcc -o 03_test 03_test.c

# 4. 加载模块
echo "4. 加载模块..."
sudo insmod 03_proc_interface.ko

echo "5. 查看创建的proc目录..."
ls -la /proc/proc_demo/

echo "6. 查看模块信息..."
cat /proc/proc_demo/info

echo "7. 运行测试程序..."
./03_test auto

echo "8. 卸载模块..."
sudo rmmod 03_proc_interface

echo "9. 清理文件..."
make clean
rm -f 03_test

echo "=== 实验3完成！ ==="
