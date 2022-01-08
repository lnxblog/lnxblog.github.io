---
layout: post
title: Memory organization of strings in C
---

One of the most common uses of the character data type is the creation of an array of characters also known as strings.
Strings can be declared and initialized in two ways.
### Method 1 ###

```c
char *a = "hello";
```
This declares a pointer to a string literal. The contents at the address pointed to by the pointer cannot be modified as it exists in the read only part of the memory of the process.


In the first method a is a pointer containing the address to the read only part of the memory containing 'hello'.
Here 'a' is a pointer to the string literal "hello".

I have compiled the code for the ARMv7 architecture, a Raspberry Pi 3B to be specific.
Here's the disassembly of the program 

```
000083e4 <main>:
    83e4:	e52db004 	push	{fp}		; (str fp, [sp, #-4]!)
    83e8:	e28db000 	add	fp, sp, #0
    83ec:	e24dd00c 	sub	sp, sp, #12
    83f0:	e59f3010 	ldr	r3, [pc, #16]	; 8408 <main+0x24>
    83f4:	e50b3008 	str	r3, [fp, #-8]
    83f8:	e1a00003 	mov	r0, r3
    83fc:	e24bd000 	sub	sp, fp, #0
    8400:	e49db004 	pop	{fp}		; (ldr fp, [sp], #4)
    8404:	e12fff1e 	bx	lr
    8408:	0000847c 	andeq	r8, r0, ip, ror r4
	
00008478 <_IO_stdin_used>:
    8478:	00020001 	andeq	r0, r2, r1
    847c:	6c6c6568 	cfstr64vs	mvdx6, [ip], #-416	; 0xfffffe60
    8480:	0000006f 	andeq	r0, r0, pc, rrx
```
As you can see in line 5, register R3 is loaded with the value stored at address 0x8408 which is the address to the string in the read only part of the program.
In line 6 this address to the string is stored on the stack, in other words the pointer 'a' is created.

### Method 2 ###
In the second method an array is created on the stack to where the characters of the string are copied.
```c
char b[6] = "hello";
```
This declares an array of characters on the stack. The contents of array can be modified as it is on the stack which is permitted for read and write.

```
000083e4 <main>:
    83e4:	e52db004 	push	{fp}		; (str fp, [sp, #-4]!)
    83e8:	e28db000 	add	fp, sp, #0
    83ec:	e24dd00c 	sub	sp, sp, #12
    83f0:	e59f2020 	ldr	r2, [pc, #32]	; 8418 <main+0x34>
    83f4:	e24b300c 	sub	r3, fp, #12
    83f8:	e8920003 	ldm	r2, {r0, r1}
    83fc:	e5830000 	str	r0, [r3]
    8400:	e2833004 	add	r3, r3, #4
    8404:	e1c310b0 	strh	r1, [r3]
    8408:	e1a00003 	mov	r0, r3
    840c:	e24bd000 	sub	sp, fp, #0
    8410:	e49db004 	pop	{fp}		; (ldr fp, [sp], #4)
    8414:	e12fff1e 	bx	lr
    8418:	0000848c 	andeq	r8, r0, ip, lsl #9

00008488 <_IO_stdin_used>:
    8488:	00020001 	andeq	r0, r2, r1
    848c:	6c6c6568 	cfstr64vs	mvdx6, [ip], #-416	; 0xfffffe60
    8490:	0000006f 	andeq	r0, r0, pc, rrx
```
In line 4 the stack size is increased. In line 5 register R2 is loaded with address to the string. R3 is loaded with address to top of stack `r3 = fp - 12`.
R0 and R1 are loaded with the string values.In line 8, R0 contents are written to the stack. Similarly halfword of R1 is stored onto stack.

## Array of strings ##

Array of strings can be declared in two methods.
### Method 1 ###
```c
char a[2][6]={"hello","world"};
printf("%p %s",a[0],a[0]);
```
On executing this program you will see the following output.
```
0x7fff80afe6b0 hello
```
When you run the program a second time you may see a different output.

```
0x7fff48b07260 hello
```
This is because the array is being created on the stack. The `hello` string is copied to the stack as depicted in method 2. So the address contained in `a[0]` is of the stack memory which is created during runtime and hence random.

### Method 2 ###

```c
char *a[2]={"hello","world"};
printf("%p %p %s",a,*a,*a);
```

Output
```
0x7fffe7f4bae0 0x400604 hello
```

When its a run a second time.
```
0x7fff8973fe60 0x400604 hello
```

So the program creates an array of pointers on the stack, where each pointer contains the address of `hello` string in the read only data section of the program.

Be sure to leave your thoughts in the comment section below.

