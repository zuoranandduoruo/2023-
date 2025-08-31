#!/bin/bash

# Linux内核进程监控模块演示脚本
# 适用于Ubuntu 20.04.6

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}  Linux内核进程监控模块演示${NC}"
    echo -e "${BLUE}================================${NC}"
    echo ""
}

print_step() {
    echo -e "${GREEN}[步骤] $1${NC}"
}

print_info() {
    echo -e "${YELLOW}[信息] $1${NC}"
}

print_error() {
    echo -e "${RED}[错误] $1${NC}"
}

check_prerequisites() {
    print_step "检查系统环境和依赖"
    
    if [ "$EUID" -eq 0 ]; then
        print_error "请不要以root用户运行此脚本"
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        print_error "make命令未找到，请安装build-essential"
        echo "sudo apt-get install build-essential"
        exit 1
    fi
    
    if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
        print_error "内核头文件未找到，请安装linux-headers"
        echo "sudo apt-get install linux-headers-$(uname -r)"
        exit 1
    fi
    
    print_info "系统环境检查通过"
    echo "内核版本: $(uname -r)"
    echo "编译器版本: $(gcc --version | head -n1)"
    echo ""
}

compile_module() {
    print_step "编译内核模块"
    
    if [ -f "process_monitor.ko" ]; then
        print_info "清理旧的编译文件"
        make clean
    fi
    
    print_info "开始编译..."
    if make all; then
        print_info "编译成功！"
        ls -la process_monitor.ko
    else
        print_error "编译失败"
        exit 1
    fi
    echo ""
}

load_module() {
    print_step "加载内核模块"
    
    if lsmod | grep -q "process_monitor"; then
        print_info "模块已加载，先卸载旧模块"
        sudo rmmod process_monitor
    fi
    
    print_info "加载新模块..."
    if sudo insmod process_monitor.ko; then
        print_info "模块加载成功！"
        lsmod | grep process_monitor
        
        print_info "检查proc文件系统接口"
        if [ -f "/proc/process_monitor" ]; then
            print_info "/proc/process_monitor 接口创建成功"
            ls -la /proc/process_monitor
        else
            print_error "/proc/process_monitor 接口创建失败"
            exit 1
        fi
    else
        print_error "模块加载失败"
        exit 1
    fi
    echo ""
}

compile_test_program() {
    print_step "编译测试程序"
    
    if [ -f "test_program" ]; then
        rm -f test_program
    fi
    
    print_info "编译用户态测试程序..."
    if gcc -o test_program test_program.c; then
        print_info "测试程序编译成功！"
        ls -la test_program
    else
        print_error "测试程序编译失败"
        exit 1
    fi
    echo ""
}

run_basic_test() {
    print_step "运行基础功能测试"
    
    print_info "清空统计信息"
    echo "clear" | sudo tee /proc/process_monitor > /dev/null
    
    print_info "查看初始状态"
    cat /proc/process_monitor
    echo ""
    
    print_info "创建测试进程..."
    ./test_program auto &
    TEST_PID=$!
    
    sleep 8
    
    print_info "查看监控结果"
    cat /proc/process_monitor
    echo ""
    
    wait $TEST_PID
    
    print_info "等待进程完全退出后再次查看"
    sleep 2
    cat /proc/process_monitor
    echo ""
}

run_interactive_test() {
    print_step "运行交互式测试"
    
    print_info "启动交互式测试程序"
    print_info "你可以选择不同的测试选项来观察内核模块的行为"
    echo ""
    
    ./test_program
}

monitor_system_activity() {
    print_step "监控系统活动"
    
    print_info "清空统计信息"
    echo "clear" | sudo tee /proc/process_monitor > /dev/null
    
    print_info "监控10秒钟的系统进程活动..."
    print_info "在另一个终端中运行一些命令来观察效果"
    
    for i in {1..10}; do
        echo -n "监控中... ${i}/10 "
        sleep 1
        echo ""
    done
    
    print_info "监控结果："
    cat /proc/process_monitor
    echo ""
}

demonstrate_features() {
    print_step "演示模块功能特性"
    
    echo "1. 进程创建监控"
    echo "2. 进程退出监控"  
    echo "3. 统计信息收集"
    echo "4. proc文件系统接口"
    echo "5. 内存管理和并发控制"
    echo "6. 系统调用拦截(kprobes)"
    echo ""
    
    print_info "功能演示完成，详细信息请查看源代码注释"
}

cleanup() {
    print_step "清理和卸载"
    
    if lsmod | grep -q "process_monitor"; then
        print_info "卸载内核模块"
        sudo rmmod process_monitor
    fi
    
    print_info "清理编译文件"
    make clean
    
    if [ -f "test_program" ]; then
        rm -f test_program
    fi
    
    print_info "清理完成"
}

show_usage() {
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  demo     - 运行完整演示"
    echo "  compile  - 仅编译模块和测试程序"
    echo "  test     - 运行自动化测试"
    echo "  interactive - 运行交互式测试"
    echo "  monitor  - 监控系统活动"
    echo "  cleanup  - 清理和卸载"
    echo "  help     - 显示此帮助信息"
    echo ""
}

main() {
    print_header
    
    case "${1:-demo}" in
        "demo")
            check_prerequisites
            compile_module
            load_module
            compile_test_program
            run_basic_test
            demonstrate_features
            echo -e "${GREEN}演示完成！使用 '$0 cleanup' 来清理环境${NC}"
            ;;
        "compile")
            check_prerequisites
            compile_module
            compile_test_program
            ;;
        "test")
            run_basic_test
            ;;
        "interactive")
            run_interactive_test
            ;;
        "monitor")
            monitor_system_activity
            ;;
        "cleanup")
            cleanup
            ;;
        "help")
            show_usage
            ;;
        *)
            print_error "未知选项: $1"
            show_usage
            exit 1
            ;;
    esac
}

trap cleanup EXIT

main "$@"
