---
layout: post
title: Software Interrupt routine on ARM  
---
Software interrupts are synchronous interrupts. They are provided by almost all architectures as an instruction. It is generally used in the implementation of system calls. In the ARM ISA the instruction `svc` is used to invoke a software interrupt which causes the system to enter `supervisor mode` and execute the vector routine for the same. Here i will provide a concise explanation of the software interrupt routine for the ARM v7 or ARM 32-bit architecture.

***arch/arm/kernel/entry-common.S***
```
ENTRY(vector_swi)

	[ ... ]

	adr tbl, sys_call_table				@ load syscall table pointer
	bic scno, scno, #0xff000000			@ mask off SWI op-code
	eor scno, scno, #__NR_SYSCALL_BASE		@ check OS number
	cmp scno, #NR_syscalls				@ check upper syscall limit
	badr lr, ret_fast_syscall			@ return address
	ldrcc pc, [tbl, scno, lsl #2]			@ call sys_* routine 
	
	[ ... ]
	
ENDPROC(vector_swi)

	[ ... ]
	#define NATIVE(nr, func) syscall nr, func
	syscall_table_start sys_call_table		@ table begins
	#define COMPAT(nr, native, compat) syscall nr, native
	#ifdef CONFIG_AEABI
	#include <calls-eabi.S>
	#else
	#include <calls-oabi.S>
	#endif
	#undef COMPAT
	syscall_table_end sys_call_table		@ table ends
	
	[ ... ]
```
The `vector_swi` routine code contains multiple `if` checks and I have shown here only the part related to loading the syscall address from `sys_call_table`.
Under the new ARM v7 EABI standard, the syscall number is stored in `r7` register. Here `scno` is a label for the `r7` register. The arguments for the system call are stored in registers `r0,r1,r2` and so on.
The addresses of the syscalls are held in `sys_call_table`. The program counter is loaded with the syscall address from this table.

```Note that each sys_call_table entry is four bytes, hence the syscall number is multipled by 4 and added to the base address of the sys_call_table.```

***calls-eabi.S***
```
NATIVE(0, sys_restart_syscall)
NATIVE(1, sys_exit)
NATIVE(2, sys_fork)
NATIVE(3, sys_read)
NATIVE(4, sys_write)

[ ... ]
```
The syscall routines are static functions whose addresses make up the `sys_call_table`. Taking the fork system call as an example,the `sys_call_table` entry is `sys_fork`. The definition of this routine is held in `linux/fork.c`. The syscall definitions like the fork here is independent of processor architecture of the underlying system.

***kernel/fork.c***
```
#ifdef __ARCH_WANT_SYS_FORK
SYSCALL_DEFINE0(fork)
{
#ifdef CONFIG_MMU
	return _do_fork(SIGCHLD, 0, 0, NULL, NULL, 0);
#else
	/* can not support in nommu mode */
	return -EINVAL;
#endif
```
Ultimately the core details of the forking process is defined in ```_do_fork```
The ```SYSCALL_DEFINE0``` is macro specifying that this system call accepts zero arguments. The macro is defined in ```syscalls.h```.

***include/linux/syscalls.h***
```
#define SYSCALL_DEFINE0(sname)					\
	SYSCALL_METADATA(_##sname, 0);				\
	asmlinkage long sys_##sname(void)
```
Similarly for one argument ```SYSCALL_DEFINE1``` is used and so on. A maximum of six arguments is allowed. The file contains the definitions for the other macros which I haven't mentioned here.
This is my walkthrough of the software interrupt routine. Let me know if you have any questions or corrections.
