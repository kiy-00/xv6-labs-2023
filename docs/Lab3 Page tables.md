# Lab3 Page tables

[TOC]

## 前置知识

### 页表

* 由于我阅读的是中文版的xv6-book，所以章节设置好像很原版有点不一样，按理来说这一章应该是阅读页表相关知识，但是这些笔记被我记到第二篇实验报告里了，所以这里不再赘述。

### 源码分析

#### `kernel/memlayout.h`

##### 代码中文注释及原理分析

```c
// 物理内存布局

// qemu -machine virt 的内存布局如下：
// 基于 qemu 的 hw/riscv/virt.c：
//
// 00001000 -- 引导 ROM，由 qemu 提供
// 02000000 -- CLINT (Core Local Interruptor, 核心局部中断控制器)
// 0C000000 -- PLIC (Platform-Level Interrupt Controller, 平台级中断控制器)
// 10000000 -- uart0 串行通信接口
// 10001000 -- virtio 虚拟磁盘接口
// 80000000 -- 引导 ROM 在机器模式下跳转到这里
//             - 内核加载到此处
// 80000000 之后的未使用的 RAM。

// 内核使用的物理内存布局如下：
// 80000000 -- entry.S, 内核文本和数据段
// end -- 内核页面分配区域的起始位置
// PHYSTOP -- 内核使用的 RAM 结束位置

// qemu 将 UART 寄存器映射到物理内存中的这个位置。
#define UART0 0x10000000L
#define UART0_IRQ 10 // UART0 的中断号

// virtio 虚拟磁盘的 MMIO 接口
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1 // virtio 的中断号

#ifdef LAB_NET
#define E1000_IRQ 33 // E1000 网卡的中断号 (用于 LAB_NET 实验)
#endif

// 核心局部中断控制器 (CLINT)，包含定时器。
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid)) // 核心 hartid 的定时比较器地址
#define CLINT_MTIME (CLINT + 0xBFF8) // 自启动以来的时钟周期数

// qemu 将平台级中断控制器 (PLIC) 映射到这个地址。
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0) // 中断优先级寄存器
#define PLIC_PENDING (PLIC + 0x1000) // 挂起的中断
#define PLIC_MENABLE(hart) (PLIC + 0x2000 + (hart)*0x100) // 核心 hart 的机器模式中断使能寄存器
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100) // 核心 hart 的监督模式中断使能寄存器
#define PLIC_MPRIORITY(hart) (PLIC + 0x200000 + (hart)*0x2000) // 核心 hart 的机器模式中断优先级阈值寄存器
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000) // 核心 hart 的监督模式中断优先级阈值寄存器
#define PLIC_MCLAIM(hart) (PLIC + 0x200004 + (hart)*0x2000) // 核心 hart 的机器模式中断声明寄存器
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000) // 核心 hart 的监督模式中断声明寄存器

// 内核期望 RAM 可供内核和用户页面使用，
// 从物理地址 0x80000000 到 PHYSTOP。
#define KERNBASE 0x80000000L // 内核的基址
#define PHYSTOP (KERNBASE + 128*1024*1024) // 内核使用的 RAM 的结束地址 (128MB)

// 将跳板页面映射到最高地址，
// 在用户空间和内核空间中都使用相同的映射。
#define TRAMPOLINE (MAXVA - PGSIZE) // 跳板页的地址

// 将内核栈映射在跳板页之下，
// 每个内核栈都被无效的保护页包围。
#define KSTACK(p) (TRAMPOLINE - (p)*2*PGSIZE - 3*PGSIZE) // 内核栈的虚拟地址

// 用户内存布局。
// 地址从零开始：
//   文本段
//   原始数据段和 bss 段
//   固定大小的栈
//   可扩展的堆
//   ...
//   USYSCALL (与内核共享)
//   TRAPFRAME (p->trapframe, 由跳板使用)
//   TRAMPOLINE (与内核中的跳板页面相同)
#define TRAPFRAME (TRAMPOLINE - PGSIZE) // 用户进程的 Trapframe 地址
#ifdef LAB_PGTBL
#define USYSCALL (TRAPFRAME - PGSIZE) // 用户系统调用的地址

struct usyscall {
  int pid;  // 进程 ID
};
#endif
```

##### 原理分析

###### 内存布局

1. **物理内存布局**：代码定义了 RISC-V QEMU 模拟器中各个硬件组件（如 CLINT、PLIC、UART 等）的物理地址。QEMU 在这些地址上映射了硬件设备的寄存器，内核可以通过访问这些地址与硬件进行交互。

2. **内核使用的内存区域**：内核加载到 `0x80000000` 地址，并使用该地址到 `PHYSTOP`（即 128MB 的 RAM）的区域来存储内核代码、数据和页面分配区域。

3. **虚拟内存布局**：定义了内核和用户空间的内存布局。跳板页 (`TRAMPOLINE`) 和 `TRAPFRAME` 页用于系统调用的上下文切换。`KSTACK` 用于为每个内核进程分配内核栈。

4. **USYSCALL**：用于实验加速系统调用的内存布局。它将 `ugetpid()` 等系统调用的结果存储在共享页面中，以减少用户空间和内核之间的切换次数。

###### 页表映射

在实验中，`mappages` 函数用于将物理内存映射到虚拟地址上。通过这种映射，内核可以有效管理内存，并确保不同进程之间的内存隔离和保护。

###### 共享页面的用途

通过共享页面，某些系统调用（如 `getpid`）可以在不切换到内核的情况下直接从用户空间访问数据。这种优化减少了系统调用的开销，提高了性能。

###### 实验要求

- 在进程创建时，将一个只读页面映射到 `USYSCALL` 虚拟地址，并初始化 `struct usyscall` 以存储当前进程的 PID。
- 测试 `ugetpid()` 是否能够正确使用该共享页面并通过测试用例。

## 实验内容

### Speed up system calls(easy)

#### 任务

* 有些操作系统（例如 Linux）通过在用户空间和内核之间共享数据的只读区域来加速某些系统调用。这消除了在执行这些系统调用时进行内核切换的需要。为了帮助你学习如何在页表中插入映射，你的第一个任务是在 xv6 中实现这个优化，用于 `getpid()` 系统调用。

  当每个进程被创建时，将一个只读页面映射到 `USYSCALL`（一个在 `memlayout.h` 中定义的虚拟地址）。在这个页面的开始部分，存储一个 `struct usyscall`（也在 `memlayout.h` 中定义），并将其初始化为当前进程的 PID。在用户空间一侧，已经提供了 `ugetpid()`，它将自动使用 `USYSCALL` 映射。若在运行 `pgtbltest` 时 `ugetpid` 测试用例通过，你将获得此实验部分的满分。

#### 分析

* 这个实验的原理就是，将一些数据存放到一个只读的共享空间中，这个空间位于内核和用户之间。这样用户程序就不用陷入内核中，而是直接从这个只读的空间中获取数据，省去了一些系统开销，加速了一些系统调用。这次的任务是改进 `getpid()` 。

  当每一个进程被创建，映射一个只读的页在 **USYSCALL** （在`memlayout.h`定义的一个虚拟地址）处。存储一个 `struct usyscall` （定义在 `memlayout.h`）结构体在该页的开始处，并且初始化这个结构体来保存当前进程的 PID。这个 lab 中，`ugetpid()` 已经在用户空间给出，它将会使用 **USYSCALL** 这个映射。运行 `pgtbltest` ，如果正确，`ugetpid` 这一项将会通过。

#### 更改`kernel/proc.h`中的`struct proc`

* 首先在 `kernel/proc.h` proc 结构体中添加一项指针来保存这个共享页面的地址。

```c
struct proc {
...
  struct usyscall *usyscallpage;  // share page whithin kernel and user
...
}
```

#### 为共享页面分配空间

* 之后需要在 `kernel/proc.c` 的 `allocproc()` 中为其分配空间(`kalloc`)。并初始化其保存当前进程的PID。

```c
static struct proc*
allocproc(void) {
...
  if ((p->usyscallpage = (struct usyscall *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  p->usyscallpage->pid = p->pid;
  
  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;
...
}
```

#### 将映射（PTE）写入 pagetable 中

* 然后在 `kernel/proc.c` 的 `proc_pagetable(struct proc *p)` 中将这个映射（PTE）写入 pagetable 中。权限是用户态可读。

```c
pagetable_t
proc_pagetable(struct proc *p) {
...
    if(mappages(pagetable, USYSCALL, PGSIZE, (uint64)(p->usyscallpage), PTE_R | PTE_U) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }
  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
...
}
```

#### 释放该共享页

* 之后要确保释放进程的时候，能够释放该共享页。同样在 `kernel/proc.c` 中的 `freeproc(struct proc *p)` 。

```c
static void
freeproc(struct proc *p) {
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  // add start
  if(p->usyscallpage)
    kfree((void *)p->usyscallpage);
  p->usyscallpage = 0;
  // add end
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
}
```

#### 修改`PTE`映射

* 在 `pagetable` 中任然存在我们之前的 PTE 映射。我们需要在 `kernel/proc.c` 的 `proc_freepagetable(pagetable_t pagetable, uint64 sz)` 中对其取消映射。

```c
void
proc_freepagetable(pagetable_t pagetable, uint64 sz) {
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmunmap(pagetable, USYSCALL, 1, 0); // add
  uvmfree(pagetable, sz);
}
```

