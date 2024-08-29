#include "kernel/types.h" // 包含通用数据类型的定义
#include "kernel/stat.h"  // 包含文件状态结构和相关常量的定义
#include "user/user.h"    // 包含用户空间程序的系统调用声明

// new_proc 函数用于在管道中创建一个新进程来处理素数筛选
void new_proc(int p[2])
{
    int prime; // 用于存储当前素数的变量
    int n;     // 用于存储从管道读取的数字

    // 关闭管道的写端，因为当前进程只需要读取数据
    close(p[1]);

    // 从管道中读取第一个数字（预期是一个素数）
    if (read(p[0], &prime, 4) != 4)
    {
        // 如果读取失败，打印错误信息并退出
        fprintf(2, "Error in read.\n");
        exit(1);
    }

    // 打印当前进程找到的素数
    printf("prime %d\n", prime);

    // 尝试从管道中读取下一个数字
    // 如果读取成功，说明需要进一步处理这个数字
    if (read(p[0], &n, 4) == 4)
    {
        int newfd[2]; // 为下一个进程创建一个新的管道
        pipe(newfd);

        // 创建一个新进程（父进程部分）
        if (fork() != 0)
        {
            // 父进程只需要向下一个进程发送数据，关闭新管道的读端
            close(newfd[0]);

            // 如果数字 n 不能被当前素数 prime 整除，将其写入新管道
            if (n % prime)
                write(newfd[1], &n, 4);

            // 继续从当前管道读取数字，直到所有数字处理完毕
            while (read(p[0], &n, 4) == 4)
            {
                // 只有不能被 prime 整除的数字才继续传递到下一个进程
                if (n % prime)
                    write(newfd[1], &n, 4);
            }

            // 关闭当前管道的读端和新管道的写端
            close(p[0]);
            close(newfd[1]);

            // 等待子进程完成
            wait(0);
        }
        // 子进程部分
        else
        {
            // 递归调用 new_proc，处理下一个素数筛选
            new_proc(newfd);
        }
    }
}

// 主函数，程序的入口点
int main(int argc, char *argv[])
{
    int p[2]; // 创建主进程和第一个子进程之间的管道
    pipe(p);

    // 创建子进程，用于开始筛选素数
    if (fork() == 0)
    {
        new_proc(p); // 子进程调用 new_proc 函数，开始处理素数筛选
    }
    // 父进程部分
    else
    {
        // 父进程只需要发送数据，关闭管道的读端
        close(p[0]);

        // 父进程将数字 2 到 35 写入管道，作为初始输入
        for (int i = 2; i <= 35; ++i)
        {
            if (write(p[1], &i, 4) != 4)
            {
                // 如果写入失败，打印错误信息并退出
                fprintf(2, "failed write %d into the pipe\n", i);
                exit(1);
            }
        }

        // 所有数字写入完成后，关闭管道的写端
        close(p[1]);

        // 等待子进程完成
        wait(0);

        // 程序正常结束
        exit(0);
    }

    return 0;
}