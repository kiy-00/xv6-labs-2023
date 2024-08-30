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
// 定义sys_pgaccess系统调用，用于检测哪些页面被访问过
int
sys_pgaccess(void)
{
  // 声明变量
  uint64 vaddr;     // 用户虚拟地址
  int num;          // 要检查的页面数
  uint64 res_addr;  // 用于存储结果的用户空间地址

  // 从用户空间获取三个参数
  argaddr(0, &vaddr);    // 获取第一个参数：虚拟地址
  argint(1, &num);       // 获取第二个参数：页面数量
  argaddr(2, &res_addr); // 获取第三个参数：结果存储地址

  struct proc *p = myproc();   // 获取当前进程指针
  pagetable_t pagetable = p->pagetable;  // 获取当前进程的页表指针
  uint64 res = 0;   // 初始化结果变量，表示哪些页面被访问过的位掩码

  // 遍历需要检查的页面
  for(int i = 0; i < num; i++){
    // 获取当前页面的页表项（PTE）
    pte_t* pte = walk(pagetable, vaddr + PGSIZE * i, 1);

    // 检查PTE中的访问位PTE_A，如果被访问过
    if(*pte & PTE_A){
      *pte &= (~PTE_A);   // 清除访问位，表示已经记录了访问
      res |= (1L << i);   // 设置结果位掩码中相应的位，表示这个页面被访问过
    }
  }

  // 将结果从内核空间拷贝到用户空间
  copyout(pagetable, res_addr, (char*)&res, sizeof(uint64));

  return 0;  // 返回0，表示系统调用成功
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


