# Lab5 Copy-on-Write Fork for xv6

[TOC]

## 实验内容

### Implement copy-on-write fork(hard)

#### 任务

* 在这个实验中，你的任务是为 xv6 实现 Copy-on-Write (COW) 的 `fork()` 系统调用。COW `fork()` 的目标是延迟物理内存页的分配和复制，直到这些页面真正需要时才执行，从而优化内存使用。
* xv6 操作系统中原来对于 fork()的实现是将父进程的用户空间全部复制到子进程的用户空间。但如果父进程地址空间太大，那这个复制过程将非常耗时。另外，现实中经常出现 fork() + exec() 的调用组合，这种情况下 fork()中进行的复制操作完全是浪费。基于此，我们可以利用页表实现写时复制机制。

#### 思路

* COW `fork()` 的实现思路是：在 `fork()` 时不立即复制物理内存页，而是让子进程的页表指向父进程的物理页，并将这些页标记为只读。当父进程或子进程尝试写这些页时，会触发页面错误，内核会在页面错误处理程序中分配一个新页，复制原来的页到新页，并更新页表，使其指向新页并允许写入。

#### 改写`kernel/vm.c`中的`uvmcopy()`

* 在 xv6 的 fork 函数中，会调用 uvmcopy 函数给子进程分配页面，并将父进程的地址空间里的内容拷贝给子进程。改写 uvmcopy 函数，不再给子进程分配页面，而是将父进程的物理页映射进子进程的页表，并将两个进程的 PTE_W 都清零。

```c
// Just declare the variables from kernel/kalloc.c
extern int useReference[PHYSTOP/PGSIZE];
extern struct spinlock ref_count_lock;


// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  // char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    // PAY ATTENTION!!!
    // 只有父进程内存页是可写的，才会将子进程和父进程都设置为COW和只读的；否则，都是只读的，但是不标记为COW，因为本来就是只读的，不会进行写入
    // 如果不这样做，父进程内存只读的时候，标记为COW，那么经过缺页中断，程序就可以写入数据，于原本的不符合
    if (*pte & PTE_W) {
      // set PTE_W to 0
      *pte &= ~PTE_W;
      // set PTE_RSW to 1
      // set COW page
      *pte |= PTE_RSW;
    }
    pa = PTE2PA(*pte);

    // increment the ref count
    acquire(&ref_count_lock);
    useReference[pa/PGSIZE] += 1;
    release(&ref_count_lock);

    flags = PTE_FLAGS(*pte);
    // if((mem = kalloc()) == 0)
    //   goto err;
    // memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      // kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

##### 代码原理说明

1. **页表项复制与COW机制**：
   - 这个函数的主要任务是将父进程的页表项复制到子进程的页表中，但与传统的 `fork()` 不同，COW机制下并不会立即为子进程分配新的物理内存页。相反，父进程和子进程共享相同的物理页面。
   - 为了实现这一点，父进程的页表项会被修改，将页表项中的写标志位 (`PTE_W`) 清除，从而将页面设置为只读，同时设置COW标志 (`PTE_COW`)。
2. **引用计数**：
   - 在 `uvmcopy()` 函数中，每个被共享的物理页面的引用计数会被增加。引用计数是用于跟踪某个物理页面被多少个进程共享。当引用计数减少到0时，该物理页面可以被安全地释放。
3. **页面错误处理**：
   - 当父进程或子进程尝试写入一个被标记为COW的页面时，由于页面被设置为只读，会触发页面错误。此时，内核会捕获该错误，为进程分配一个新的物理页面，将原页面内容复制到新页面中，然后更新页表项，使其指向新页面，并设置写标志位。这就是COW机制的核心：只有在实际写操作发生时，才会进行页面的复制，从而节省内存。
4. **错误处理**：
   - 如果在为子进程映射页面的过程中发生错误，例如 `mappages()` 失败，那么 `uvmcopy()` 会解除子进程中已完成的映射，并返回错误代码 `-1`。

#### 编写 COW handler

* 此时父子进程对所有的 COW 页都没有写权限，如果某个进程试图对某个页进行写，就会触发 `page fault(scause = 15)`，因此需要在 `trap.c/usertrap` 中处理这个异常。

```c
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  }
  else if (r_scause() == 15) {
    // Store/AMO page fault(write page fault) and Load page fault
    // see Volume II: RISC-V Privileged Architectures V20211203 Page 71
    
    // the faulting virtual address
    // see Volume II: RISC-V Privileged Architectures V20211203 Page 41
    // the download url is https://github.com/riscv/riscv-isa-manual/releases/download/Priv-v1.12/riscv-privileged-20211203.pdf
    uint64 va = r_stval();
    if (va >= p->sz)
      p->killed = 1;
    int ret = cowhandler(p->pagetable, va);
    if (ret != 0)
      p->killed = 1;
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}
```

* 我们会检查 scause 寄存器的值是否是 15，如果是的话就调用 cowhandler 函数。

```c
int
cowhandler(pagetable_t pagetable, uint64 va)
{
    char *mem;
    if (va >= MAXVA)
      return -1;
    pte_t *pte = walk(pagetable, va, 0);
    if (pte == 0)
      return -1;
    // check the PTE
    if ((*pte & PTE_RSW) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_V) == 0) {
      return -1;
    }
    if ((mem = kalloc()) == 0) {
      return -1;
    }
    // old physical address
    uint64 pa = PTE2PA(*pte);
    // copy old data to new mem
    memmove((char*)mem, (char*)pa, PGSIZE);
    // PAY ATTENTION
    // decrease the reference count of old memory page, because a new page has been allocated
    kfree((void*)pa);
    uint flags = PTE_FLAGS(*pte);
    // set PTE_W to 1, change the address pointed to by PTE to new memory page(mem)
    *pte = (PA2PTE(mem) | flags | PTE_W);
    // set PTE_RSW to 0
    *pte &= ~PTE_RSW;
    return 0;
}

```

* cowhandler 做的事情也很简单，它首先会检查一系列权限位，然后分配一个新的物理页，并将它映射到产生缺页异常的进程的页表中，同时设置写权限位。

#### 增加物理页计数器(`kalloc.c`)

* 由于现在可能有多个进程拥有同一个物理页，如果某个进程退出时 free 掉了这个物理页，那么其他进程就会出错。所以我们得设置一个全局数组，记录每个物理页被几个进程所拥有。同时注意这个数组可能会被多个进程同时访问，因此需要用一个锁来保护。

```c
// the reference count of physical memory page
int useReference[PHYSTOP/PGSIZE];
struct spinlock ref_count_lock;
```

* 每个物理页所对应的计数器将在下面几个函数内被修改：

* 首先在 kalloc 分配物理页函数中将对应计数器置为 1

```c
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    acquire(&ref_count_lock);
    // initialization the ref count to 1
    useReference[(uint64)r / PGSIZE] = 1;
    release(&ref_count_lock);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

* 进程在 fork 时会调用 uvmcopy 函数，我们要在其中将 COW 页对应的计数器加 1。

* 另外在某个进程想 free 掉某个物理页时，我们要将其计数器减 1。

```c
// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int temp;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref_count_lock);
  // decrease the reference count, if use reference is not zero, then return
  useReference[(uint64)pa/PGSIZE] -= 1;
  temp = useReference[(uint64)pa/PGSIZE];
  release(&ref_count_lock);
  if (temp > 0)
    return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

```

#### 修改 `kernel/vm.c`中的`copyout`

* 最后，如果内核调用 copyout 函数试图修改一个进程的 COW 页，也需要进行 cowhandler 类似的操作来处理。

```c
// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;

    struct proc *p = myproc();
    pte_t *pte = walk(pagetable, va0, 0);
    if (*pte == 0)
      p->killed = 1;
    // check
    if (checkcowpage(va0, pte, p)) 
    {
      char *mem;
      if ((mem = kalloc()) == 0) {
        // kill the process
        p->killed = 1;
      }else {
        memmove(mem, (char*)pa0, PGSIZE);
        // PAY ATTENTION!!!
        // This statement must be above the next statement
        uint flags = PTE_FLAGS(*pte);
        // decrease the reference count of old memory that va0 point
        // and set pte to 0
        uvmunmap(pagetable, va0, 1, 1);
        // change the physical memory address and set PTE_W to 1
        *pte = (PA2PTE(mem) | flags | PTE_W);
        // set PTE_RSW to 0
        *pte &= ~PTE_RSW;
        // update pa0 to new physical memory address
        pa0 = (uint64)mem;
      }
    }
    
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```

#### 在`kernel/riscv.h`中添加宏定义

```c
#define PTE_RSW (1L << 8) // RSW
```

#### 添加头文件

* 在`kernel/vm.c`中添加`#include "proc.h"`以及`#inlude "spinlock.h"`

#### 添加函数`checkcowpage`

* 在`kernel/vm.c`中添加：

```c
int checkcowpage(uint64 va, pte_t *pte, struct proc* p) {
  return (va < p->sz) // va should blow the size of process memory (bytes)
    && (*pte & PTE_V) 
    && (*pte & PTE_RSW); // pte is COW page
}
```

#### 测试成功

<img src="img/cowtest.png" alt="cowtest" style="zoom:67%;" />