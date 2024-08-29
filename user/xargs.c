#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

// 定义一个宏，用于调试时输出信息
#define DEBUG 0

// 如果开启了调试模式（DEBUG为1），则执行其中的代码
#define debug(codes) \
    if (DEBUG)       \
    {                \
        codes        \
    }

// 函数声明
void xargs_exec(char *program, char **paraments);

void xargs(char **first_arg, int size, char *program_name)
{
    char buf[1024]; // 用于存储从标准输入读取的数据
    debug(
        for (int i = 0; i < size; ++i) {
            printf("first_arg[%d] = %s\n", i, first_arg[i]);
        }) char *arg[MAXARG]; // 用于存储命令及其参数
    int m = 0;

    // 从标准输入逐字符读取数据
    while (read(0, buf + m, 1) == 1)
    {
        if (m >= 1024)
        {
            fprintf(2, "xargs: arguments too long.\n");
            exit(1);
        }
        // 当读取到换行符时，说明读到了一行
        if (buf[m] == '\n')
        {
            buf[m] = '\0'; // 将换行符替换为字符串结束符
            debug(printf("this line is %s\n", buf);)

                // 将命令行参数复制到 arg 数组中
                memmove(arg, first_arg, sizeof(*first_arg) * size);

            // 设置参数索引
            int argIndex = size;
            if (argIndex == 0)
            { // 如果没有提供初始参数，则将命令名作为第一个参数
                arg[argIndex] = program_name;
                argIndex++;
            }

            // 将读取的行添加到参数数组中
            arg[argIndex] = malloc(sizeof(char) * (m + 1));
            memmove(arg[argIndex], buf, m + 1);
            debug(
                for (int j = 0; j <= argIndex; ++j)
                    printf("arg[%d] = *%s*\n", j, arg[j]);)

                // exec 的参数需要以 0 结尾
                arg[argIndex + 1] = 0;

            // 调用函数执行命令
            xargs_exec(program_name, arg);

            // 释放分配的内存
            free(arg[argIndex]);
            m = 0; // 重置读取缓冲区位置
        }
        else
        {
            m++;
        }
    }
}

void xargs_exec(char *program, char **paraments)
{
    if (fork() > 0)
    { // 父进程等待子进程完成
        wait(0);
    }
    else
    {
        debug(
            printf("child process\n");
            printf("    program = %s\n", program);

            for (int i = 0; paraments[i] != 0; ++i) {
                printf("    paraments[%d] = %s\n", i, paraments[i]);
            })
            // 子进程执行命令
            if (exec(program, paraments) == -1)
        {
            fprintf(2, "xargs: Error exec %s\n", program);
        }
        debug(printf("child exit");)
    }
}

int main(int argc, char *argv[])
{
    debug(printf("main func\n");) char *name = "echo"; // 默认命令为 echo
    if (argc >= 2)
    {
        name = argv[1]; // 如果命令行参数中指定了命令，则使用指定的命令
        debug(
            printf("argc >= 2\n");
            printf("argv[1] = %s\n", argv[1]);)
    }
    else
    {
        debug(printf("argc == 1\n");)
    }
    // 调用 xargs 函数执行
    xargs(argv + 1, argc - 1, name);
    exit(0);
}