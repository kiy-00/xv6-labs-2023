#include "kernel/types.h" // 包含标准数据类型定义
#include "kernel/stat.h"  // 包含文件状态和系统调用相关定义
#include "user/user.h"    // 包含用户空间程序所需的系统调用声明

int main(int argc, char *argv[])
{
    // 用于在父进程和子进程之间通信的管道
    int fd[2];

    // 创建管道，fd[0] 是读端，fd[1] 是写端
    if (pipe(fd) == -1)
    {
        // 如果创建管道失败，输出错误信息并终止程序
        fprintf(2, "Error: pipe(fd) error.\n");
        exit(1); // 使用非零值退出表示错误
    }

    // 创建子进程
    if (fork() == 0)
    {
        // 子进程代码块
        char buffer[1];
        // 从管道的读端读取一个字节
        read(fd[0], buffer, 1);
        // 关闭不需要再使用的读端
        close(fd[0]);
        // 输出子进程收到的信息
        fprintf(0, "%d: received ping\n", getpid());
        // 将刚收到的字节通过管道写回父进程
        write(fd[1], buffer, 1);
        // 关闭不再需要的写端
        close(fd[1]);
        // 子进程正常退出
        exit(0);
    }
    else
    {
        // 父进程代码块
        char buffer[1];
        buffer[0] = 'a'; // 准备发送给子进程的字符
        // 将字符写入管道，发送给子进程
        write(fd[1], buffer, 1);
        // 关闭写端，因为已经完成发送
        close(fd[1]);
        // 从管道的读端读取子进程发回的数据
        read(fd[0], buffer, 1);
        // 输出父进程收到的信息
        fprintf(0, "%d: received pong\n", getpid());
        // 关闭不再需要的读端
        close(fd[0]);
    }

    // 程序正常退出
    exit(0);
}