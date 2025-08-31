#!/bin/bash

# 实验4执行脚本 - 并发控制与锁机制

echo "========== 实验4：并发控制与锁机制 ========="

# 1. 修改Makefile
echo "1. 配置编译..."
sed -i 's/obj-m += .*/obj-m += 04_concurrency_control.o/' Makefile

# 2. 编译
echo "2. 编译模块..."
make all

# 3. 编译测试程序
echo "3. 编译测试程序..."
gcc -o 04_test 04_test.c

# 4. 加载模块
echo "4. 加载模块..."
sudo insmod 04_concurrency_control.ko

echo "5. 查看初始状态..."
cat /proc/concurrency_demo

echo "6. 测试各种锁机制..."
echo "spin 100" | sudo tee /proc/concurrency_demo > /dev/null
echo "mutex 200" | sudo tee /proc/concurrency_demo > /dev/null
echo "rwlock 300" | sudo tee /proc/concurrency_demo > /dev/null

echo "7. 启动工作线程进行并发测试..."
echo "start" | sudo tee /proc/concurrency_demo > /dev/null
sleep 5

echo "8. 查看并发测试结果..."
cat /proc/concurrency_demo

echo "9. 停止工作线程..."
echo "stop" | sudo tee /proc/concurrency_demo > /dev/null

echo "10. 卸载模块..."
sudo rmmod 04_concurrency_control

echo "11. 清理文件..."
make clean
rm -f 04_test

echo "=== 实验4完成！ ==="
