#!/bin/bash

# 实验5执行脚本 - 完整进程监控系统

echo "========== 实验5：完整进程监控系统 ========="

# 1. 修改Makefile
echo "1. 配置编译..."
sed -i 's/obj-m += .*/obj-m += 05_complete_monitor.o/' Makefile

# 2. 编译
echo "2. 编译模块..."
make all

# 3. 编译测试程序
echo "3. 编译测试程序..."
gcc -o 05_test 05_test.c

# 4. 加载模块
echo "4. 加载模块..."
sudo insmod 05_complete_monitor.ko

echo "5. 查看监控系统接口..."
ls -la /proc/process_monitor_complete/

echo "6. 查看初始统计信息..."
cat /proc/process_monitor_complete/stats

echo "7. 创建测试进程..."
for i in 1 2 3 4 5; do
    sleep 1 &
done
sleep 3

echo "8. 查看进程监控结果..."
cat /proc/process_monitor_complete/stats

echo "9. 查看进程记录..."
cat /proc/process_monitor_complete/processes | head -15

echo "10. 测试过滤功能..."
echo "comm sleep" | sudo tee /proc/process_monitor_complete/filter > /dev/null
echo "enable" | sudo tee /proc/process_monitor_complete/filter > /dev/null
echo "过滤后的进程列表:"
cat /proc/process_monitor_complete/processes | head -10

echo "11. 重置过滤器..."
echo "reset" | sudo tee /proc/process_monitor_complete/filter > /dev/null

echo "12. 最终统计信息..."
cat /proc/process_monitor_complete/stats

echo "13. 卸载模块..."
sudo rmmod 05_complete_monitor

echo "14. 清理文件..."
make clean
rm -f 05_test

echo "=== 实验5完成！ ==="
