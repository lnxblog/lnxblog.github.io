---
layout: post
title: Page tables in Linux
---
Linux employs virtual addresses instead of physical addresses to isolate each process's address space and provide greater security. But ultimately accessing the physical memory will require that the memory controller be provided with a physical address to access the physical memory.

 In order to achieve this, Linux stores a table of mappings containing virtual to physical memory mappings of each process. But this is not a simple one-to-one mapping as this would require a lot of space in the physical memory. Instead Linux uses a 3 page table approach. The virtual address bits are seperated into four distinct fields each of which helps in naviagting the page table structures
The 3 page tables are
1. Page Global Directory (PGD)
2. Page Middle Directory (PMD)
3. Page Table Entries (PTE)

Recently Linux has been updated to handle another page table between PGD and PMD called Page Upper Direcory (PUD). Note that page table implementation is architecture specific.
But for the case of ARM 32 bit, there are only two page tables used, PGD and PTE as we will shortly see.

## Walking the Table ##
The following diagram helps explain the page table layout better.

![Linux Page tables](https://www.kernel.org/doc/gorman/html/understand/understand-html006.png)

PGD is an array of `pgd_t` members, PMD is an array of `pmd_t` members and PTE is an array of `pte_t` members.
Each process is represented by a `task_struct` structure which contains a pointer to `mm_struct`, another structure containing information on the virtual memory areas of the process. Within `mm_struct` a pointer to its own Page Global Directory exists.
Before reading the page tables, we acquire the page table lock member of the `mm_struct` using spinlock API.
The kernel API offers methods required to find the offset within each table.
The offset within the PGD table is obtained by right shifting the bits of the virtual address by `PGDIR_SHIFT` and adding the value to the PGD pointer.
```
pgd_offset(mm, addr) *** mm is the `mm_struct` and addr is the virtual address
```
From the previous step we have the PUD entry.
To find the offset within PUD the `pud_offset(pgd_offset_entry, addr)` is used, but for ARMv7 there is no PUD table and this function simply returns the pgd offset entry from the previous step.

Next we have PMD table and the `pmd_offset(pud_offset_entry, addr)` provides the offset entry but as I observed that this value was simply the return of the pud_offset_entry which is in turn the pgd_offset_entry.

Finally the PTE table provides the pte_t entry containing the address to the `page` struct. The function `pte_offset_map(pmd_offset_entry, addr)` is used to handle this.
Once we have located the page containing our address, we have to find the offset of the location within the page. Each page in ARMv7 is configured to be of 4096 bytes or 2^12. We find the offset within the page by using the lowermost 12 bits.

## Experiment ##

I wanted to experiment with page tables and wrote a kernel module to perform page table traversal. It works as follows
1. Provide the process ID (PID) and address of a variable to the kernel module's device descriptor. This can be done using the `echo` command.
2. The kernel module obtains the `task_struct` using the PID.
3. Obtain the `mm_struct` and the pgd pointer. Traverse the page tables using the page table offset APIs.
4. Find the page struct from the PTE entry 
5. Perform temporary mapping of the page using `kmap_atomic` and find offset within page from the virtual address.
5. Read the value at the address and print the contents.

### Test code ###
I used the following simple code and will try to read the contents of variable `a`
```C
#include <stdio.h>

int main()
{
	int a=8055;
	printf("%p\n",&a);
	while(1);
}
```
Running the program will print the address of `a`
```
$ ./test_code &
[1] 614
0x7e9e6c24
```

The kernel module creates a char device interface to read the user arguments. Usage is as follows
```
$ echo "614 7e9e6c24 " > /dev/test_driver

[  122.536785] test driver device opened
[  122.537051] test driver device write 
[  122.537227] user writes : 614 7e9e6c24
[  122.537227]
[  122.537364] user 1st write 614
[  122.537384] user 2nd write 7e9e6c24
[  122.537462] task_struct pid : 614
[  122.537532] virt addr :2124311588 7e9e6c24
[  122.537621] pgd pointer: c8058000 pgd entry:c8059fa0
[  122.537745] pud pointer: c8059fa0
[  122.537812] pmd pointer: c8059fa0
[  122.537880] pte pointer: c8269798
[  122.537948] page address 574a1000 physical address 574a1c24
[  122.537974] value at address : 8055
[  122.538079] test driver device closed / released
```
I have uploaded the driver code [here](https://github.com/lnxblog/Page-Table-Walk/blob/master/page_table_walk.c).


References
1. [Linux Page Table Management](https://www.kernel.org/doc/gorman/html/understand/understand006.html)
2. [Tim's Notes on ARM memory allocation](https://elinux.org/Tims_Notes_on_ARM_memory_allocation)


