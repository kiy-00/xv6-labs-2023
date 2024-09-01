# Lab9 file system

[TOC]

## 前置知识

## 实验内容

### Large files ([moderate](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

#### 任务

* 目前，xv6文件系统中的每个inode包含12个“直接”块号和一个“单一间接”块号，后者引用一个块，该块最多可以包含256个块号，总共可以引用12+256=268个块。因此，xv6文件的最大大小限制为268个块。

  你需要修改xv6文件系统代码，支持每个inode中的“双重间接”块，这样文件的最大大小可以增加到65803个块，即256*256+256+11个块（因为我们将牺牲一个直接块号以用于双重间接块）。

#### 理解文件系统结构

- `struct dinode`（定义在`fs.h`中）描述了磁盘上的inode结构。你需要关注`NDIRECT`、`NINDIRECT`、`MAXFILE`以及`struct dinode`中的`addrs[]`元素。
- `bmap()`函数位于`fs.c`中，用于查找文件在磁盘上的数据块。`bmap()`会根据需要分配新块以容纳文件内容，并在需要时分配间接块来存储块地址。



#### 步骤

##### 1. 增加双重间接块支持

- **原始代码**:

  ```c
  //kernel/fs.c
  static uint
  bmap(struct inode *ip, uint bn)
  {
    uint addr, *a;
    struct buf *bp;
  
    if(bn < NDIRECT){
      if((addr = ip->addrs[bn]) == 0){
        addr = balloc(ip->dev);
        if(addr == 0)
          return 0;
        ip->addrs[bn] = addr;
      }
      return addr;
    }
    bn -= NDIRECT;
  
    if(bn < NINDIRECT){
      // 加载单一间接块，如有需要则进行分配。
      if((addr = ip->addrs[NDIRECT]) == 0){
        addr = balloc(ip->dev);
        if(addr == 0)
          return 0;
        ip->addrs[NDIRECT] = addr;
      }
      bp = bread(ip->dev, addr);
      a = (uint*)bp->data;
      if((addr = a[bn]) == 0){
        addr = balloc(ip->dev);
        if(addr){
          a[bn] = addr;
          log_write(bp);
        }
      }
      brelse(bp);
      return addr;
    }
    panic("bmap: out of range");
  }
  ```

  - 在原始代码中，`bmap` 函数只支持最多 12 个直接块和 1 个单一间接块。这限制了文件的最大大小为 268 个块。

- **修改后代码**:

  ```c
  //kernel/fs.c
  static uint
  bmap(struct inode *ip, uint bn)
  {
    uint addr, *a;
    struct buf *bp;
  
    if(bn < NDIRECT){
      if((addr = ip->addrs[bn]) == 0)
        ip->addrs[bn] = addr = balloc(ip->dev);
      return addr;
    }
    bn -= NDIRECT;
  
    if(bn < NINDIRECT){
      // 加载单一间接块，如有需要则进行分配。
      if((addr = ip->addrs[NDIRECT]) == 0)
        ip->addrs[NDIRECT] = addr = balloc(ip->dev);
      bp = bread(ip->dev, addr);
      a = (uint*)bp->data;
      if((addr = a[bn]) == 0){
        a[bn] = addr = balloc(ip->dev);
        log_write(bp);
      }
      brelse(bp);
      return addr;
    }
    bn -= NINDIRECT;
  
    // 处理双重间接块的情况
    if(bn < NDINDIRECT) {
      int level2_idx = bn / NADDR_PER_BLOCK;  // 二级间接块的位置
      int level1_idx = bn % NADDR_PER_BLOCK;  // 一级间接块的位置
      // 读取二级间接块
      if((addr = ip->addrs[NDIRECT + 1]) == 0)
        ip->addrs[NDIRECT + 1] = addr = balloc(ip->dev);
      bp = bread(ip->dev, addr);
      a = (uint*)bp->data;
  
      if((addr = a[level2_idx]) == 0) {
        a[level2_idx] = addr = balloc(ip->dev);
        log_write(bp);
      }
      brelse(bp);
  
      bp = bread(ip->dev, addr);
      a = (uint*)bp->data;
      if((addr = a[level1_idx]) == 0) {
        a[level1_idx] = addr = balloc(ip->dev);
        log_write(bp);
      }
      brelse(bp);
      return addr;
    }
    panic("bmap: out of range");
  }
  ```

  - 修改后的代码添加了对双重间接块的支持，从而允许文件的最大大小扩展至 65,803 个块。该修改通过将一个直接块的位置用于双重间接块，从而增加了文件系统的容量。

  **目的**: 支持更大文件尺寸。通过引入双重间接块，文件系统可以管理比之前更大的文件。

##### 2. 增加双重间接块的截断支持

- **原始代码**:

  ```c
  void
  itrunc(struct inode *ip)
  {
    int i, j;
    struct buf *bp;
    uint *a;
  
    for(i = 0; i < NDIRECT; i++){
      if(ip->addrs[i]){
        bfree(ip->dev, ip->addrs[i]);
        ip->addrs[i] = 0;
      }
    }
  
    if(ip->addrs[NDIRECT]){
      bp = bread(ip->dev, ip->addrs[NDIRECT]);
      a = (uint*)bp->data;
      for(j = 0; j < NINDIRECT; j++){
        if(a[j])
          bfree(ip->dev, a[j]);
      }
      brelse(bp);
      bfree(ip->dev, ip->addrs[NDIRECT]);
      ip->addrs[NDIRECT] = 0;
    }
    
    ip->size = 0;
    iupdate(ip);
  }
  ```

  - 原始代码只处理直接块和单一间接块的截断操作。

- **修改后代码**:

  ```c
  void
  itrunc(struct inode *ip)
  {
    int i, j;
    struct buf *bp;
    uint *a;
  
    for(i = 0; i < NDIRECT; i++){
      if(ip->addrs[i]){
        bfree(ip->dev, ip->addrs[i]);
        ip->addrs[i] = 0;
      }
    }
  
    if(ip->addrs[NDIRECT]){
      bp = bread(ip->dev, ip->addrs[NDIRECT]);
      a = (uint*)bp->data;
      for(j = 0; j < NINDIRECT; j++){
        if(a[j])
          bfree(ip->dev, a[j]);
      }
      brelse(bp);
      bfree(ip->dev, ip->addrs[NDIRECT]);
      ip->addrs[NDIRECT] = 0;
    }
    
    // 处理双重间接块的截断
    struct buf* bp1;
    uint* a1;
    if(ip->addrs[NDIRECT + 1]) {
      bp = bread(ip->dev, ip->addrs[NDIRECT + 1]);
      a = (uint*)bp->data;
      for(i = 0; i < NADDR_PER_BLOCK; i++) {
        if(a[i]) {
          bp1 = bread(ip->dev, a[i]);
          a1 = (uint*)bp1->data;
          for(j = 0; j < NADDR_PER_BLOCK; j++) {
            if(a1[j])
              bfree(ip->dev, a1[j]);
          }
          brelse(bp1);
          bfree(ip->dev, a[i]);
        }
      }
      brelse(bp);
      bfree(ip->dev, ip->addrs[NDIRECT + 1]);
      ip->addrs[NDIRECT + 1] = 0;
    }
    
    ip->size = 0;
    iupdate(ip);
  }
  ```

  - 修改后的代码支持双重间接块的截断操作，确保当文件被删除或缩小时，所有分配的块都能被正确释放。

  **目的**: 使得文件系统能够正确管理更大文件的块分配，并在文件被截断时释放所有相关的块。

##### 3. `kernel/fs.h`中`NDIRECT`的修改

  - **原文件**: `#define NDIRECT 12`
  - **修改后**: `#define NDIRECT 11`
  - **目的**: 减少直接块的数量，从而为新增的双重间接块腾出空间。

##### 4. 增加`NDINDIRECT`和`NADDR_PER_BLOCK`宏定义

  - 修改后新增:

    ```c
    //kernel/fs.h
    #define NDINDIRECT ((BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)))
    #define NADDR_PER_BLOCK (BSIZE / sizeof(uint))
    ```

  - 目的:

    - `NDINDIRECT` 用于定义双重间接块可以引用的总块数。
- `NADDR_PER_BLOCK` 用于计算一个块中可以存储的地址数量。

##### 5. 修改MAXFILE的计算

- **原文件**: `#define MAXFILE (NDIRECT + NINDIRECT)`
- **修改后**: `#define MAXFILE (NDIRECT + NINDIRECT + NDINDIRECT)`
- **目的**: 将 `MAXFILE` 的计算范围扩展到包含双重间接块，这样可以支持更大的文件。

##### 6. **增加Inode中`addrs`数组的大小**

- **原文件**: `uint addrs[NDIRECT+1];`
- **修改后**: `uint addrs[NDIRECT+2];`
- **目的**: 扩展`addrs`数组的大小以支持双重间接块。

  #### `kernel/fs.h`修改内容原理解释

  在原始的文件系统设计中，`inode` 包含了 `NDIRECT` 个直接块地址和 1 个单重间接块地址，这意味着一个文件最多可以使用 12 个直接块和 256 个通过单一间接块引用的块，总共 268 个块。

  在修改后的设计中，通过减少一个直接块的数量（即将 `NDIRECT` 从 12 减少到 11），我们腾出了一个新的地址项，用于存储一个双重间接块的地址。双重间接块使得文件系统可以通过多层间接块引用更多的磁盘块，从而大大增加了文件的最大支持大小。新的`MAXFILE`值反映了这一变化，最大文件大小从268个块增加到65,803个块。

  具体来说，双重间接块使用了两级索引来指向数据块。第一级索引块包含指向第二级索引块的地址，而第二级索引块又包含指向数据块的地址。这个设计允许文件系统支持极大的文件尺寸，符合更大数据需求的应用场景。

#### 测试成功



### Symbolic links ([moderate](https://pdos.csail.mit.edu/6.828/2023/labs/guidance.html))

#### 任务

* 这个实验的目标是为xv6操作系统增加对符号链接（symbolic links）的支持。符号链接是一种特殊的文件类型，它通过路径名引用另一个文件。当打开符号链接时，内核会跟随链接指向的文件。符号链接类似于硬链接，但硬链接只能指向同一磁盘上的文件，而符号链接可以跨磁盘设备引用文件。虽然xv6不支持多个设备，但实现这个系统调用是理解路径名查找工作原理的好练习。

* 实现`symlink`系统调用：

  **任务描述：** 你将实现`symlink(char *target, char *path)`系统调用，它在`path`位置创建一个新的符号链接，指向`target`文件。你需要确保在实现过程中符号链接能够正确地处理，尤其是处理路径名查找的递归和循环检测。

#### 步骤

**创建新系统调用：**

- 为`symlink`创建一个新的系统调用编号，并在`user/usys.pl`和`user/user.h`中添加对应的条目。
- 在`kernel/sysfile.c`中实现一个空的`sys_symlink`函数。

```c
// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
//here
int symlink(char*, char*);
```

```c
entry("fork");
entry("exit");
entry("wait");
entry("pipe");
entry("read");
entry("write");
entry("close");
entry("kill");
entry("exec");
entry("open");
entry("mknod");
entry("unlink");
entry("fstat");
entry("link");
entry("mkdir");
entry("chdir");
entry("dup");
entry("getpid");
entry("sbrk");
entry("sleep");
entry("uptime");
//here
entry("symlink");
```

**定义符号链接文件类型：**

- 在`kernel/stat.h`中添加一个新的文件类型`T_SYMLINK`，用于表示符号链接。

```c
#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device
#define T_SYMLINK 4   // Soft symbolic link - lab 9.2

struct stat {
  int dev;     // File system's disk device
  uint ino;    // Inode number
  short type;  // Type of file
  short nlink; // Number of links to file
  uint64 size; // Size of file in bytes
};
```

**添加新标志：**

- 在`kernel/fcntl.h`中添加一个新的标志`O_NOFOLLOW`，用于`open`系统调用。当指定该标志时，`open`应该打开符号链接本身，而不是跟随符号链接指向的文件。确保该标志不会与现有标志冲突。

```c
#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400
#define O_NOFOLLOW 0x004    // lab 9.2
```

**实现`symlink`系统调用：**

- 在`path`位置创建一个新的符号链接，链接到`target`。符号链接的目标路径可以存储在inode的数据块中。
- `symlink`应返回一个整数，表示成功（0）或失败（-1），类似于`link`和`unlink`系统调用。

```c
// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}
```

**修改`open`系统调用：**

- 修改`open`系统调用，以处理路径指向符号链接的情况。如果文件不存在，`open`应当失败。
- 如果`open`指定了`O_NOFOLLOW`标志，应当打开符号链接本身，而不是跟随符号链接。
- 如果符号链接指向的文件也是符号链接，必须递归地跟随直到达到非链接文件。如果符号链接形成了循环，应当返回错误代码。你可以通过设置一个递归深度的阈值（例如10）来近似处理这个问题。

```c
uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  // handle the symlink - lab 9.2
  if(ip->type == T_SYMLINK && (omode & O_NOFOLLOW) == 0) {
    if((ip = follow_symlink(ip)) == 0) {
      // 此处不用调用iunlockput()释放锁
      // follow_symlinktest()返回失败时,锁在函数内已经被释放
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

// Generating symbolic links
uint64 
sys_symlink(void) {
  char target[MAXPATH], path[MAXPATH];
  struct inode *ip;
  int n;

  if ((n = argstr(0, target, MAXPATH)) < 0
    || argstr(1, path, MAXPATH) < 0) {
    return -1;
  }

  begin_op();
  // create the symlink's inode
  if((ip = create(path, T_SYMLINK, 0, 0)) == 0) {
    end_op();
    return -1;
  }
  // write the target path to the inode
  if(writei(ip, 0, (uint64)target, 0, n) != n) {
    iunlockput(ip);
    end_op();
    return -1;
  }

  iunlockput(ip);
  end_op();
  return 0;
}
```

**确保其他系统调用的正确性：**

- 修改其他相关的系统调用（如`link`和`unlink`），使它们操作符号链接本身，而不是跟随符号链接指向的文件。

```c
// recursively follow the symlinks - lab9-2
// Caller must hold ip->lock
// and when function returned, it holds ip->lock of returned ip
static struct inode* 
follow_symlink(struct inode* ip) {
  uint inums[NSYMLINK];
  int i, j;
  char target[MAXPATH];

  for(i = 0; i < NSYMLINK; ++i) {
    inums[i] = ip->inum;
    // read the target path from symlink file
    if(readi(ip, 0, (uint64)target, 0, MAXPATH) <= 0) {
      iunlockput(ip);
      printf("open_symlink: open symlink failed\n");
      return 0;
    }
    iunlockput(ip);
    
    // get the inode of target path 
    if((ip = namei(target)) == 0) {
      printf("open_symlink: path \"%s\" is not exist\n", target);
      return 0;
    }
    for(j = 0; j <= i; ++j) {
      if(ip->inum == inums[j]) {
        printf("open_symlink: links form a cycle\n");
        return 0;
      }
    }
    ilock(ip);
    if(ip->type != T_SYMLINK) {
      return ip;
    }
  }

  iunlockput(ip);
  printf("open_symlink: the depth of links reaches the limit\n");
  return 0;
}
```

### 实验得分

