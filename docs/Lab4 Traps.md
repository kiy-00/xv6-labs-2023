# Lab4 Traps

[TOC]

## 前置知识

## 实验内容

### RISC-V assembly (easy)

#### 任务

* 理解RISC-V汇编代码。

* 分析函数调用中的寄存器使用情况。

* 理解little-endian和big-endian的差异及其在编程中的影响。

#### 生成汇编代码

在你的xv6仓库中，`user/call.c` 文件已经存在。运行以下命令：

```bash
make fs.img
```

这个命令会编译代码并生成程序的可读汇编版本，文件位于 `user/call.asm`。

#### 阅读`call.asm`

```c
void main(void) {
  1c:	1141                	addi	sp,sp,-16
  1e:	e406                	sd	ra,8(sp)
  20:	e022                	sd	s0,0(sp)
  22:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  24:	4635                	li	a2,13
  26:	45b1                	li	a1,12
  28:	00000517          	auipc	a0,0x0
  2c:	7b850513          	addi	a0,a0,1976 # 7e0 <malloc+0xe6>
  30:	00000097          	auipc	ra,0x0
  34:	612080e7          	jalr	1554(ra) # 642 <printf>
  exit(0);
  38:	4501                	li	a0,0
  3a:	00000097          	auipc	ra,0x0
  3e:	28e080e7          	jalr	654(ra) # 2c8 <exit>
```

* ==Q1: Which registers contain arguments to functions? For example, which register holds 13 in main's call to `printf`?==

* A: 通过阅读 `call.asm` 文件中的 `main` 函数可知，调用 `printf` 函数时，`13` 被寄存器 `a2` 保存。`a1`, `a2`, `a3` 等通用寄存器；`13` 被寄存器 `a2` 保存。

```c
int g(int x) {
   0:	1141                	addi	sp,sp,-16
   2:	e422                	sd	s0,8(sp)
   4:	0800                	addi	s0,sp,16
  return x+3;
}
   6:	250d                	addiw	a0,a0,3
   8:	6422                	ld	s0,8(sp)
   a:	0141                	addi	sp,sp,16
   c:	8082                	ret

000000000000000e <f>:

int f(int x) {
   e:	1141                	addi	sp,sp,-16
  10:	e422                	sd	s0,8(sp)
  12:	0800                	addi	s0,sp,16
  return g(x);
}
  14:	250d                	addiw	a0,a0,3
  16:	6422                	ld	s0,8(sp)
  18:	0141                	addi	sp,sp,16
  1a:	8082                	ret
```

* ==Q2: Where is the call to function `f` in the assembly code for main? Where is the call to `g`? (Hint: the compiler may inline functions.)==

* 通过阅读函数 `f` 和 `g` 得知：函数 `f` 调用函数 `g` ；函数 `g` 使传入的参数加 3 后返回。

* 所以总结来说，函数 `f` 就是使传入的参数加 3 后返回。考虑到编译器会进行内联优化，这就意味着一些显而易见的，编译时可以计算的数据会在编译时得出结果，而不是进行函数调用。

  查看 `main` 函数可以发现，`printf` 中包含了一个对 `f` 的调用。

```c
 printf("%d %d\n", f(8)+1, 13);
  24:	4635                	li	a2,13
  26:	45b1                	li	a1,12
```

* 但是对应的会汇编代码却是直接将 `f(8)+1` 替换为 `12` 。这就说明编译器对这个函数调用进行了优化，所以对于 `main` 函数的汇编代码来说，其并没有调用函数 `f` 和 `g` ，而是在运行之前由编译器对其进行了计算。
* A: `main` 的汇编代码没有调用 `f` 和 `g` 函数。编译器对其进行了优化。

```c
void
printf(const char *fmt, ...)
{
 642:	711d                	addi	sp,sp,-96
 644:	ec06                	sd	ra,24(sp)
 646:	e822                	sd	s0,16(sp)
 648:	1000                	addi	s0,sp,32
 64a:	e40c                	sd	a1,8(s0)
 64c:	e810                	sd	a2,16(s0)
 64e:	ec14                	sd	a3,24(s0)
 650:	f018                	sd	a4,32(s0)
 652:	f41c                	sd	a5,40(s0)
 654:	03043823          	sd	a6,48(s0)
 658:	03143c23          	sd	a7,56(s0)
  va_list ap;

  va_start(ap, fmt);
 65c:	00840613          	addi	a2,s0,8
 660:	fec43423          	sd	a2,-24(s0)
  vprintf(1, fmt, ap);
 664:	85aa                	mv	a1,a0
 666:	4505                	li	a0,1
 668:	00000097          	auipc	ra,0x0
 66c:	dce080e7          	jalr	-562(ra) # 436 <vprintf>
}
```

* ==Q3: At what address is the function `printf` located?==
* A: `0x642`

* `auipc` 和 `jalr` 的配合，可以跳转到任意 32 位的地址。

```c
30:	00000097          	auipc	ra,0x0
34:	612080e7          	jalr	1554(ra) # 642 <printf>
```

* 第 49 行，使用 `auipc ra,0x0` 将当前程序计数器 `pc` 的值存入 `ra` 中。

  第 50 行，`jalr 1554(ra)` 跳转到偏移地址 `printf` 处，也就是 `0x642` 的位置。

  根据 [reference1](https://xiayingp.gitbook.io/build_a_os/hardware-device-assembly/risc-v-assembly) 中的信息，在执行完这句命令之后， 寄存器 `ra` 的值设置为 `pc + 4` ，也就是 `return address` 返回地址 `0x38`。

* ==Q4: What value is in the register `ra` just after the `jalr` to `printf` in `main`?==

* A: `0x38`

#### 运行指定代码

<img src="img/test.png" alt="test" style="zoom:67%;" />

* 观察输出：

<img src="img/output.png" alt="output" style="zoom:67%;" />

```bash
He110 World
```

* 首先，`57616` 转换为 16 进制为 `e110`，所以格式化描述符 `%x` 打印出了它的 16 进制值。

  其次，如果在小端（little-endian）处理器中，数据`0x00646c72` 的**高字节存储在内存的高位**，那么从**内存低位**，也就是**低字节**开始读取，对应的 ASCII 字符为 `rld`。

  如果在 大端（big-endian）处理器中，数据 `0x00646c72` 的**高字节存储在内存的低位**，那么从**内存低位**，也就是**高字节**开始读取其 ASCII 码为 `dlr`。

  所以如果大端序和小端序输出相同的内容 `i` ，那么在其为大端序的时候，`i` 的值应该为 `0x726c64`，这样才能保证从内存低位读取时的输出为 `rld` 。

  无论 `57616` 在大端序还是小端序，它的二进制值都为 `e110` 。大端序和小端序只是改变了多字节数据在内存中的存放方式，并不改变其真正的值的大小，所以 `57616` 始终打印为二进制 `e110` 。

* ==Q5: The output depends on that fact that the RISC-V is little-endian. If the RISC-V were instead big-endian what would you set `i` to in order to yield the same output? Would you need to change `57616` to a different value?==

* A: 如果在大端序，`i` 的值应该为 `0x726c64` 才能保证与小端序输出的内容相同。不用该变 `57616` 的值。

* ==Q6: In the following code, what is going to be printed after `'y='`? (note: the answer is not a specific value.) Why does this happen?==

```c
printf("x=%d y=%d", 3);
```

* 通过之前的章节可知，函数的参数是通过寄存器`a1`, `a2` 等来传递。如果 `prinf` 少传递一个参数，那么其仍会从一个确定的寄存器中读取其想要的参数值，但是我们并没有给出这个确定的参数并将其存储在寄存器中，所以函数将从此寄存器中获取到一个随机的不确定的值作为其参数。故而此例中，`y=`后面的值我们不能够确定，它是一个垃圾值。
* A: `y=` 之后的值为一个不确定的垃圾值。