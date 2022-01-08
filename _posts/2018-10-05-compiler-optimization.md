---
layout: post
title: Compiler optimization in GCC 
---

The GCC provides various levels of compiler optimization. The option can be specified by `-On` where n contains integral values {0,1,2,3}.

`-O0`
Reduce compilation time and make debugging produce the expected results. This is the default.

`-O1`
Low level optimization. The compiler tries to reduce code size and execution time, without performing any optimizations that take a great deal of compilation time.

`-O2`
Medium level optimization. GCC performs nearly all supported optimizations that do not involve a space-speed tradeoff. As compared to -O1, this option increases both compilation time and the performance of the generated code.

`-O3`
High level optimization. -O3 turns on all optimizations specified by -O2 and some additional optimizations, which increases the total compilation time.

Consider the following C code snippet:
```c 
int flag = 1;
void main()
{
	while(flag);
}	
```

The following is the disassembly for an ARM architecture based device like the Raspberry Pi.
It is compiled with `-O0` optimization.
```
    8424:	e59f301c 	ldr	r3, [pc, #28]	; 8448 <main+0x30>
    8428:	e5933000 	ldr	r3, [r3]
    842c:	e3530000 	cmp	r3, #0
    8430:	1afffffb 	bne	8424 <main+0xc>
```
Register `R3` is loaded with the global variable `flag`'s value. It is compared with zero and if not equal it branches to the `0x8424` to repeat the procedure again.

The following is the disassembly under `O3` optimization.
```
    82f4:	e59f3020 	ldr	r3, [pc, #32]	; 831c <main+0x28>
    82f8:	e92d4010 	push	{r4, lr}
    82fc:	e5934000 	ldr	r4, [r3]
    8300:	e3540000 	cmp	r4, #0
    8304:	0a000000 	beq	830c <main+0x18>
    8308:	eafffffe 	b	8308 <main+0x14>
```
Note the instruction at `0x8308` the code branches to itself which although seems odd is the correct behavior expected from our program. It is faster too as there is no memory address being accessed like the previous code.

We can avoid this kind of optimization by using a function in another source file to return the `flag` variable value.

file1.c
```c
int main()
{
	while(check_var());
}
```

file2.c
```c
int flag=1;
int check_var()
{
	return flag;
}
```
Now when after compiling the above two source files to produce the executable and disassemble `main`:

```
<main>
    [.]
    8424:	eb000008 	bl	844c <check_var>
    8428:	e1a03000 	mov	r3, r0
    842c:	e3530000 	cmp	r3, #0
    8430:	1afffffb 	bne	8424 <main+0xc>
    [.]
```
Similarly for `check_var`:

```
<check_var>
    844c:	e52db004 	push	{fp}		; (str fp, [sp, #-4]!)
    8450:	e28db000 	add	fp, sp, #0
    8454:	e59f3010 	ldr	r3, [pc, #16]	; 846c <check_var+0x20>
    8458:	e5933000 	ldr	r3, [r3]
    845c:	e1a00003 	mov	r0, r3
    8460:	e24bd000 	sub	sp, fp, #0
    8464:	e49db004 	pop	{fp}		; (ldr fp, [sp], #4)
    8468:	e12fff1e 	bx	lr
    846c:	00010614 	andeq	r0, r1, r4, lsl r6
```

Register `R0` is used to store the return value.
Now the program is forced to load the variable with the value at the memory address and is thus not optimized away.
