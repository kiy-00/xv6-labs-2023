#include "kernel/types.h" // 包含系统中常用的基本数据类型的定义
#include "kernel/stat.h"  // 包含文件状态相关的结构和常量定义
#include "user/user.h"    // 包含用户空间程序所需的系统调用声明
#include <stdlib.h>       // 包含标准库的函数声明

// 主函数入口，argc 是命令行参数的个数，argv 是参数的数组
int main(int argc, char *argv[])
{
    // 如果命令行参数不等于2（程序名和时间参数），输出使用说明并退出
    if (argc != 2)
    {
        fprintf(2, "Usage: sleep time_in_ticks\n"); // 错误输出到文件描述符2（标准错误）
        exit(1);                                    // 以错误码1退出程序
    }

    // 将字符串参数转化为整数，存储在 time 变量中
    // ++argv 将指针移动到 argv[1]，即用户输入的第一个参数，atoi 将其转换为整数
    char *endptr;
    int time = strtol(argv[1], &endptr, 10);

    // 检查转换后的结果是否合法
    if (*endptr != '\0' || time <= 0)
    {
        fprintf(2, "Error: Invalid time argument. Please enter a positive integer.\n");
        exit(1);
    }

    // 调用 sleep 系统调用，暂停执行指定的时间（tick）
    // 如果 sleep 调用失败（返回值不为0），输出错误信息
    if (sleep(time) != 0)
    {
        fprintf(2, "Error in sleep sys_call!\n"); // 错误信息输出到标准错误
    }

    // 程序执行完毕，正常退出
    exit(0);
}