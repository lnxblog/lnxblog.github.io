---
layout: post
title: Tracing mmap in Linux
---

Memory Map or mmap ia a feature in Linux that allows a process to map a device or file into its virtual address space. It is invoked via the mmap() system call. Linux adds a new virtual memory area or vm_area_struct to the address space of the process. This can be checked in the proc/<pid>/maps entry of the process.

From the man page:

```C
void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset);
```

mmap() returns a pointer to the beginning of the virtual address of the mapped area.
In case the mapping fails, MAP_FAILED is returned.

---

Internally Linux calls `sys_mmap2` routine. Below is the code for an ARM 32-bit machine. This is arch specific code. The subsequent functions are generic.

### arch/arm/kernel/entry-common.S ###
```
sys_mmap2:
#if PAGE_SHIFT > 12
		tst	r5, #PGOFF_MASK
		moveq	r5, r5, lsr #PAGE_SHIFT - 12
		streq	r5, [sp, #4]
		beq	sys_mmap_pgoff
		mov	r0, #-EINVAL
		mov	pc, lr
#else
		str	r5, [sp, #4]
		b	sys_mmap_pgoff
#endif
ENDPROC(sys_mmap2)
```
The next routine is sys_mmap_pgoff which internally invokes `ksys_mmap_pgoff` in `mm/mmap.c`. `ksys_mmap_pgoff` performs some checks before finally calling `vm_mmap_pgoff` located in `mm/util.c`

```
unsigned long ksys_mmap_pgoff(unsigned long addr, unsigned long len,
			      unsigned long prot, unsigned long flags,
			      unsigned long fd, unsigned long pgoff)
{
	[...]

	retval = vm_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	return retval;
}

SYSCALL_DEFINE6(mmap_pgoff, unsigned long, addr, unsigned long, len,
		unsigned long, prot, unsigned long, flags,
		unsigned long, fd, unsigned long, pgoff)
{
	return ksys_mmap_pgoff(addr, len, prot, flags, fd, pgoff);
}
```
Inside `vm_mmap_pgoff` mm->mmap_sem is acquired before calling `do_mmap_pgoff`  function in again in `mm/mmap.c`.

`do_mmap_pgoff` performs checks on the flags parameter and will call the device or file specific `get_unmapped_area` to allocate memory for the mapping.

```c
unsigned long do_mmap_pgoff(struct file *file, unsigned long addr,
			unsigned long len, unsigned long prot,
			unsigned long flags, unsigned long pgoff,
			unsigned long *populate)
{
	
	[...]
	
	addr = get_unmapped_area(file, addr, len, pgoff, flags);

	[...]

	addr = mmap_region(file, addr, len, vm_flags, pgoff); // addr used by vm_area_struct

	[...]
	return addr;
}
```

Next `mmap_region()` function is invoked which allocates a `vm_area_struct` from the slab allocator. 
Finally the device specific or filesystem `mmap()` function is invoked.

```c
unsigned long mmap_region(struct file *file, unsigned long addr,
		unsigned long len, vm_flags_t vm_flags, unsigned long pgoff)
{

	vma = kmem_cache_zalloc(vm_area_cachep, GFP_KERNEL);
	if (!vma) {
		error = -ENOMEM;
		goto unacct_error;
	}

	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_flags = vm_flags;
	vma->vm_page_prot = vm_get_page_prot(vm_flags);
	vma->vm_pgoff = pgoff;

	[...]
	
	error = file->f_op->mmap(file, vma);
	
	[...]
	
```

So where does the mmap routine ulimately begin.
### If a device wants to allow user to memory map the device memory area then it must have a mmap routine written as part of the driver code. ###
This doesn't apply when mapping files because Linux provides a built-in mmap routine for that.

So now I will describe a simple mmap routine in a device driver.

The intented use of mmap in a device driver is to provide  user space access to a memory area allocated inside a driver routine.
So I have allocated some memory using the `kalloc()` function in the `initfunction` of the driver and copied 'hello world' string into the area.


```C

    ptr = kmalloc(4096,GFP_KERNEL);
	if (!ptr)
	{
		printk(KERN_EMERG "memory allocation failed");
		return -ENOMEM;
	}
    strcpy(ptr,"hello world\n");
```

Next inside the mmap routine, the `remap_pfn_page` maps a contiguous physical address space represented by the `ptr` which we allocated in the `initfunction` into the virtual space represented by vm_area_struct.

```C
static ssize_t dummy_fop_mmap(struct file *filp, struct vm_area_struct *vma)
{
	long addr=ptr;
	if (remap_pfn_range(vma, vma->vm_start,virt_to_phys(addr)>>PAGE_SHIFT,4096,vma->vm_page_prot))
	{
		printk("hemanth device mmap error");
		return -EAGAIN;
	}
    return 0;

}
```
Next populate file operations structure with a pointer to the mmap routine.

```C
static const struct file_operations hemanth_fops = {
	.owner = THIS_MODULE,
	.open = dummy_fop_open,
	.release = dummy_fop_release,
	.read = dummy_fop_read,
	.write = dummy_fop_write,
	.mmap = dummy_fop_mmap
};
```

So now when we do mmap on the file descriptor for the driver from user-space, we can read get content stored in the area, which in this case is 'hello world'.

I have uploaded the driver code and user space mmap code [here](https://github.com/lnxblog/mmap_demo).

Feel free to comment.

References
1. [Linux device drivers](https://lwn.net/Kernel/LDD3/)
2. [https://linux-kernel-labs.github.io/master/labs/memory_mapping.html](https://linux-kernel-labs.github.io/master/labs/memory_mapping.html)




