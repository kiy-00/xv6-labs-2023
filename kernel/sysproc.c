#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
   uint64 va;             // 定义变量 `va`，用于存储虚拟地址
  int pagenum;           // 定义变量 `pagenum`，用于存储要检查的页面数量
  uint64 abitsaddr;      // 定义变量 `abitsaddr`，用于存储用户空间中位掩码的地址

  // 从用户空间中获取系统调用的三个参数
  argaddr(0, &va);       // 获取第一个参数（起始虚拟地址）并存储在 `va` 中
  argint(1, &pagenum);   // 获取第二个参数（页面数量）并存储在 `pagenum` 中
  argaddr(2, &abitsaddr);// 获取第三个参数（位掩码的地址）并存储在 `abitsaddr` 中

  uint64 maskbits = 0;   // 初始化 `maskbits`，用于存储页面访问情况的位掩码
  struct proc *proc = myproc(); // 获取当前进程的指针

  // 遍历每一个需要检查的页面
  for (int i = 0; i < pagenum; i++) {
    // 通过 `walk` 函数获取虚拟地址 `va + i * PGSIZE` 对应的页表条目（PTE）
    pte_t *pte = walk(proc->pagetable, va + i * PGSIZE, 0);

    // 如果页表条目不存在，则触发 panic，说明页不存在
    if (pte == 0)
      panic("page not exist.");

    // 检查页表条目中的访问位（PTE_A）是否被设置
    if (PTE_FLAGS(*pte) & PTE_A) {
      // 如果访问位被设置，将对应的位在 `maskbits` 中置 1
      maskbits = maskbits | (1L << i);
    }

    // 清除 PTE_A 访问位，将访问位置为 0
    *pte = ((*pte & PTE_A) ^ *pte) ^ 0;
  }

  // 将 `maskbits` 拷贝到用户空间指定的地址 `abitsaddr` 中
  if (copyout(proc->pagetable, abitsaddr, (char *)&maskbits, sizeof(maskbits)) < 0)
    panic("sys_pgacess copyout error"); // 如果拷贝失败，触发 panic

  return 0; // 返回 0 表示成功
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


