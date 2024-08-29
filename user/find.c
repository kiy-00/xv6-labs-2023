#include "kernel/types.h" // 包含基本数据类型定义
#include "kernel/stat.h"  // 包含文件状态相关结构和常量定义
#include "kernel/fs.h"    // 包含文件系统相关的结构和常量定义
#include "user/user.h"    // 包含用户空间程序的系统调用声明

// 去除字符串后面的空格
char *rtrim(char *path)
{
    static char newStr[DIRSIZ + 1]; // 用于存储去除空格后的字符串
    int whiteSpaceSize = 0;         // 计算路径末尾的空格数
    int bufSize = 0;                // 去除空格后的字符串长度
    // 从路径字符串末尾向前遍历，统计空格数
    for (char *p = path + strlen(path) - 1; p >= path && *p == ' '; --p)
    {
        ++whiteSpaceSize;
    }
    bufSize = DIRSIZ - whiteSpaceSize; // 计算去除空格后的有效长度
    memmove(newStr, path, bufSize);    // 将有效部分复制到新字符串中
    newStr[bufSize] = '\0';            // 添加字符串结束符
    return newStr;                     // 返回去除空格后的字符串
}

// 在目录树中查找特定文件名的文件
void find(char *path, char *name)
{
    char buf[512], *p; // 缓冲区和指针，用于构建完整的路径
    int fd;            // 文件描述符
    struct dirent de;  // 目录项结构，用于保存目录中的文件信息
    struct stat st;    // 文件状态结构，用于保存文件的详细信息

    // 打开给定路径的目录
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path); // 打开失败则输出错误信息
        return;
    }

    // 获取目录的状态信息
    if (fstat(fd, &st) == -1)
    {
        fprintf(2, "find: cannot fstat %s\n", path); // 获取状态信息失败则输出错误信息
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_DEVICE:                                        // 设备文件
    case T_FILE:                                          // 普通文件
        fprintf(2, "find: %s not a path value.\n", path); // 如果给定路径不是目录，则提示错误
        close(fd);
        break;
    case T_DIR: // 目录
        // 检查路径长度是否超出缓冲区大小
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            printf("ls: path too long\n");
            break;
        }
        // 复制路径到缓冲区，并在后面添加 '/'
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        // 读取目录中的每个目录项
        while (read(fd, &de, sizeof(de)) == sizeof de)
        {
            if (de.inum == 0) // 如果目录项为空则跳过
                continue;
            // 跳过 '.' 和 '..' 目录
            if (strcmp(".", rtrim(de.name)) == 0 || strcmp("..", rtrim(de.name)) == 0)
                continue;
            // 将目录项的文件名附加到缓冲区的路径后面
            memmove(p, de.name, DIRSIZ);
            // 创建一个以 '\0' 结尾的字符串
            p[DIRSIZ] = '\0';
            // 获取目录项对应文件的状态信息
            if (stat(buf, &st) == -1)
            {
                fprintf(2, "find: cannot stat '%s'\n", buf); // 获取失败则输出错误信息
                continue;
            }
            // 如果是文件或设备文件，且名称匹配则输出路径
            if (st.type == T_DEVICE || st.type == T_FILE)
            {
                if (strcmp(name, rtrim(de.name)) == 0)
                {
                    printf("%s\n", buf); // 输出匹配的文件路径
                }
            }
            // 如果是目录，则递归调用 find 函数继续查找
            else if (st.type == T_DIR)
            {
                find(buf, name);
            }
        }
    }
    close(fd); // 关闭文件描述符
}

// 主函数，处理命令行参数并调用 find 函数
int main(int argc, char *argv[])
{
    if (argc != 3)
    {                                           // 检查命令行参数数量
        fprintf(2, "Usage: find path file.\n"); // 参数数量不对则提示使用方法
        exit(0);
    }
    char *path = argv[1]; // 获取路径参数
    char *name = argv[2]; // 获取文件名参数
    find(path, name);     // 调用 find 函数查找文件
    exit(0);              // 退出程序
}