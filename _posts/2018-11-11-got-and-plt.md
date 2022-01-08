---
layout: post
title: GOT and PLT  
---

The Global offset table (GOT) and Procedure Linkage Table (PLT) are the two data structures which form the basis for run time resolution of symbols in the ELF standard.

The following video gives a good explanation of its usage by the dynamic linker for the Intel x86 architecture. 

[ GOT and PLT - Youtube ](https://www.youtube.com/watch?v=kUk5pw4w0h4)

In this post I will post the details of its working for the ARM architecture.
I will consider the following simple program which uses a printf statement. The printf function is defined in the libc library and the dynamic linker is responsible for resolving the printf call during runtime. 

```c 
int main()
{
	printf("hello world");
	while(1);
}	
```
The .plt section is as follows:

```
Disassembly of section .plt:

000102b4 <printf@plt-0x14>:
   102b4:       e52de004        push    {lr}            ; (str lr, [sp, #-4]!)
   102b8:       e59fe004        ldr     lr, [pc, #4]    ; 102c4 <_init+0x1c>
   102bc:       e08fe00e        add     lr, pc, lr
   102c0:       e5bef008        ldr     pc, [lr, #8]!
   102c4:       00010300        .word   0x00010300

000102c8 <printf@plt>:
   102c8:       e28fc600        add     ip, pc, #0, 12 
   102cc:       e28cca10        add     ip, ip, #16, 20 ; 0x10000  ip = ip + 16 << 20
   102d0:       e5bcf300        ldr     pc, [ip, #768]! ; 0x300
   
   [.]
 
   00010420 <main>:
   10420:       e92d4800        push    {fp, lr}
   10424:       e28db004        add     fp, sp, #4
   10428:       e24dd008        sub     sp, sp, #8
   1042c:       e59f300c        ldr     r3, [pc, #12]   ; 10440 <main+0x20>
   10430:       e50b3008        str     r3, [fp, #-8]
   10434:       e59f0004        ldr     r0, [pc, #4]    ; 10440 <main+0x20>
   10438:       ebffffa2        bl      102c8 <printf@plt>
   
   [.]
   
Disassembly of section .got:

000205c4 <_GLOBAL_OFFSET_TABLE_>:
   205c4:       000204dc        ldrdeq  r0, [r2], -ip
        ...
   205d0:       000102b4                        ; <UNDEFINED> instruction: 0x000102b4
   205d4:       000102b4                        ; <UNDEFINED> instruction: 0x000102b4
   205d8:       000102b4                        ; <UNDEFINED> instruction: 0x000102b4
   205dc:       000102b4                        ; <UNDEFINED> instruction: 0x000

```

From main printf causes branching to `0x102c8` which is the plt for printf.
1. In the first instruction the value of `pc` which is `0x102d0` to be copied into `ip`
2. The next instruction is the following expression

	`ip = ip + 16 << (32 - 12)`

3. Finally in the third instruction `pc` is loaded with the value `0x102b4` from the GOT entry for printf@plt `0x205d0`.
4. At `0x102b4` the `printf@plt-0x14` stub is used to invoke the dynamic linker.
5. It begins by pushing `lr` onto the stack.
6. Next instruction loads `lr` with content at `pc + 4`.
7. Next `lr = lr + pc`  which results in `lr = 0x10300 + 0x102c4 = 0x205c4`
8. Next pc is loaded with the value located at `lr + 8`


You can use GDB to examine the memory contents.

```
pi@raspberrypi:~/c_progs $ gdb -q a.out
Reading symbols from a.out...(no debugging symbols found)...done.
(gdb) p/x *0x205d0
$1 = 0x102b4
(gdb) p/x *0x205cc
$2 = 0x0
(gdb) b main
Breakpoint 1 at 0x10420
(gdb) r
Starting program: /home/pi/c_progs/a.out

Breakpoint 1, 0x00010420 in main ()
(gdb) p/x *0x205cc
$3 = 0x76fe5544

(gdb) info proc mappings
process 1446
Mapped address spaces:

        Start Addr   End Addr       Size     Offset objfile
           0x10000    0x11000     0x1000        0x0 /home/pi/c_progs/a.out
           0x20000    0x21000     0x1000        0x0 /home/pi/c_progs/a.out
        0x76e66000 0x76f91000   0x12b000        0x0 /lib/arm-linux-gnueabihf/libc-2.19.so
        0x76f91000 0x76fa1000    0x10000   0x12b000 /lib/arm-linux-gnueabihf/libc-2.19.so
        0x76fa1000 0x76fa3000     0x2000   0x12b000 /lib/arm-linux-gnueabihf/libc-2.19.so
        0x76fa3000 0x76fa4000     0x1000   0x12d000 /lib/arm-linux-gnueabihf/libc-2.19.so
        0x76fa4000 0x76fa7000     0x3000        0x0
        0x76fba000 0x76fbf000     0x5000        0x0 /usr/lib/arm-linux-gnueabihf/libarmmem.so
        0x76fbf000 0x76fce000     0xf000     0x5000 /usr/lib/arm-linux-gnueabihf/libarmmem.so
        0x76fce000 0x76fcf000     0x1000     0x4000 /usr/lib/arm-linux-gnueabihf/libarmmem.so
        0x76fcf000 0x76fef000    0x20000        0x0 /lib/arm-linux-gnueabihf/ld-2.19.so
---Type <return> to continue, or q <return> to quit---
        0x76ff7000 0x76ffb000     0x4000        0x0
        0x76ffb000 0x76ffc000     0x1000        0x0 [sigpage]
        0x76ffc000 0x76ffd000     0x1000        0x0 [vvar]
        0x76ffd000 0x76ffe000     0x1000        0x0 [vdso]
        0x76ffe000 0x76fff000     0x1000    0x1f000 /lib/arm-linux-gnueabihf/ld-2.19.so
        0x76fff000 0x77000000     0x1000    0x20000 /lib/arm-linux-gnueabihf/ld-2.19.so
        0x7efdf000 0x7f000000    0x21000        0x0 [stack]
        0xffff0000 0xffff1000     0x1000        0x0 [vectors]
(gdb) c
Continuing.
^C
Program received signal SIGINT, Interrupt.
0x0001043c in main ()
(gdb) p/x *0x205d0
$4 = 0x76eaf2a0
(gdb)
```

You can observe that the GOT entry for printf@plt is updated after making the first call to printf@plt-0x14. 

First call goes through printf@plt-0x14 for all functions which are part of shared object libraries. This stub invokes the dynamic linker which resolves the address of the external function like printf in this case and actually calls the printf function.

Subsequent calls directly invoke the function definition instead of invoking the dynamic linker as the address of the function is now stored in the GOT.