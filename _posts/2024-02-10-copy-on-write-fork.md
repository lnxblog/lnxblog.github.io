---
layout: post
title: Forking with Copy-on-Write
---
A fork creates a new process with an exact copy of the memory of the calling parent process. This can be an expensive operation both in terms of time and memory. 

Every process has some pages which are marked as read only. It makes sense to share these pages when forking. And for the writable pages, these can be marked read only until one of the processes decide to perform a write, at which point a copy is given to the writing process. This is the idea behind Copy-on-Write.

## Normal Fork ##

![Normal fork](/assets/images/2024/normal_fork.PNG)


## Copy-on-Write Fork ##

![COW fork](/assets/images/2024/cow_fork.PNG)


## Making COW work ##
### Fork ###
There many gotchas in making COW working correctly, as I learned. So I will try to put forth what I learnt when when working with the xv6 OS. 

The first step involves changing how fork works. In a normal fork, the copies of the parent process's pages are created and corresponding page table entries (PTE) are added to the page table. 

In COW fork, no new pages are created for the child. Instead PTEs are added which map to the parent's pages. The permission bits to writable pages are cleared making it READ ONLY.

```
PTE: | Page Physical address | Reserved bits | Read | Write | eXecute | Valid |
```
Another important step is it maintain a reference count for the page which indicates the number of processes sharing the page. So in the cow fork step, the reference count for each page is incremented.

### Page Fault ###

When one of the processes attempts to perform a store in the COW pages, a page fault is triggered. In order to distinguish between a page fault due to a valid write and an invalid write to page, we can set additional bits in the PTE to indicate this.

```
if COW bit set and Write bit not set:
		Handle COW page
```
There are additional registers provided by the hardware to hold the address of the faulting instruction and memory. We are interested in the stval register which holds the virtual address of the write. A copy of the original page is created and the PTE of the faulting process is update with the new page and Write flag enabled.

Additionally the reference count of the original page is decremented. What would happen is say the reference count becomes 1 and a process attempts to perform a write? In that since its the only owner of the page, the only action required is enabling Write flag and return.
	
### Copying from kernel space to user space ###
What would happend when the kernel tries to copy bytes to a process's memory during operations like reading from a device or a pipe? Since the COW pages of the process are marked as read only, it would lead to a page fault and as this arises from the kernel, can lead to a kernel panic. 

Handling this involves modifying the copyout function of the kernel. Similar to the page fault handler, kernel needs to check it this is a COW page and create a copy of the page and update the PTE and the reference count of the pages.

### Process Exit ###
Typically when a process exits, its pages are freed by the kernel. But in the case of COW pages, the kernel needs to decrement the reference count on each kfree and only release the page once the reference count reaches zero, which indicates that no processes are using the page.



## References ##
1. [https://pdos.csail.mit.edu/6.828/2023/xv6.html](https://pdos.csail.mit.edu/6.828/2023/xv6.html)
2. [https://lwn.net/Articles/849638/](https://lwn.net/Articles/849638/)
3. [xv6 book, Chapter 4](https://pdos.csail.mit.edu/6.828/2023/xv6/book-riscv-rev3.pdf)