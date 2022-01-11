---
layout: post
title: Using BPF to filter ping requests
---
Berkley Packet Filter (BPF) is a kernel feature in Linux used for event tracing and manipulating network packets. eBPF is the extended version of Berkley Packet Filter (BPF). It is a feature that was initial introduced for network packet filtering but now has additional functions added to it. eBPF is known as just BPF and the former BPF is known as Classical BPF (cBPF).

eBPF programs run sandboxed in a virtual machine which is part of the kernel and communicate with the userspace through a data structure called 'maps'.
eBPF programs are verfied before execution to ensure things like loops are not present in the code. Resources like CPU and memory are precious in kernel space and programs cannot overutilize them.

BPF was initially designed only for writing programs related to filtering network packets. But with eBPF it has been extended to hook kprobes into syscalls, kernel functions. This opens up the potential for designing performance analysis applications.

eBPF programs consists of two components. A user space program and kernel space program. Basically the kernel space program contains the bytecode which will run in the kernel VM. The user space program is responsible for loading the compiled kernel program object file into the kernel space and monitoring any data shared by the kernel space program through shared maps.

A popular utility is `tcpdump` which uses BPF to provide network statistics on interfaces.

Examples can be found in `samples/bpf` in the Linux kernel sources.

Writing these programs can be complex. Thankfully the BPF Compiler Collection (BCC) makes writing and building BPF programs easier. Setting up involves a simple command:
```Shell
sudo apt-get install bpfcc-tools linux-headers-$(uname -r)
```

Let us see a example program which hooks into the `clone` system call. Whenever the clone system call is executed the hello_world routine handler is executed. The routine prints the uid in the trace using the `bpf_trace_printk` helper function.

```python
#!/usr/bin/python
from bcc import BPF
from time import sleep

program = """
int hello_world(void *ctx) {
   u64 uid;

   uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;
   bpf_trace_printk("%ld\\n",uid);
   return 0;
}
"""

b = BPF(text=program)
clone = b.get_syscall_fnname("clone")	# get the function name for clone syscall
b.attach_kprobe(event=clone, fn_name="hello_world")		# Attach kprobe to the clone event and trigger function when event occurs
b.trace_print()		# spit out the traces
```
The text content declared in the `program` variable is the code which is compiled for the kernel space. The rest of the python code is converted to equivalent C code and compiled for user space. Pretty neat.

We will need to run the above python script with superuser privileges. On the console we can see trace logs like below for each occurence of the clone system call.
```
b'            bash-9460    [000] d... 228436.536682: bpf_trace_printk: 1000'
b''
b'            bash-9460    [000] d... 228447.144196: bpf_trace_printk: 1000'
b''
b' gnome-terminal--1887    [000] d... 228460.988633: bpf_trace_printk: 1000'
b''
b' gnome-terminal--1887    [000] d... 228460.989399: bpf_trace_printk: 1000'
b''
b' pool-gnome-term-9501    [000] d... 228461.014430: bpf_trace_printk: 1000'
b''
b'            bash-9503    [000] d... 228461.022127: bpf_trace_printk: 1000'
b''
b'        lesspipe-9504    [000] d... 228461.023027: bpf_trace_printk: 1000'
```

Packet filtering, manipuation and dropping is one of biggest reasons for usage of BPF by many tech companies. The popular network command line utility `traceroute` uses ICMP messages to trace all the node IPs in a path to a destination IP. For security reasons companies don't like to reveal their node IP in the path. 

Here BPF helps in dropping any ICMP packets that arrive at the NIC card interface and thus prevent any ICMP reply messages.
This is achieved through the eXpress Data Processing (XDP) utility in the BPF framework. Essentially the idea is to move the packet processing function onto the NIC card or at least the device driver of the NIC card.

I am running Ubuntu on a VM so I did not check if XDP was supported for the virtualized NIC interface. I am able to ping the VM from my Windows host machine. The objective was to see if this ping packet can be dropped using XDP on the VM.

```python

from bcc import BPF


interface = "enp0s3"

xdp_prog='''
#include <bcc/proto.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/in.h>


int pinger(struct xdp_md *ctx)
{

	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
	struct iphdr *ip;
	if ((void *)eth + sizeof(*eth) <= data_end)
	{
	
		//filter IPv4 packets (ethernet type = 0x0800)
		if (!(eth->h_proto == htons(0x0800))) {
			return XDP_DROP;
		}
		
		ip = data + sizeof(*eth);
		if ((void *)ip + sizeof(*ip) <= data_end)
		{
			// check if packet IP packet is of ICMP type, if yes then drop
			if(ip->protocol == IPPROTO_ICMP)
			{
				bpf_trace_printk("blocking icmp packet");
				return XDP_DROP;
			}
		
		}

	}
	// allow packet to pass to upper layer of network stack
	return XDP_PASS;

}
'''

bpf = BPF(text=xdp_prog)

function_pinger = bpf.load_func("pinger", BPF.XDP)
BPF.attach_xdp(interface,function_pinger,0)
bpf.trace_print()
```
The IP packet arriving at the interface has multiple layers starting with the Link layer. The first if statement checks if the packet size is lesser than the size of ethernet header. Next we check if the ethernet header encapsulates a IPv4 payload. In the network layer the IPv4 header has a protocol field to indicate the Transport layer protocol of its payload like TCP, UDP or ICMP. We check if this protocol number matches ICMP.
And if it does we go ahead and drop the packet. For all other cases the packet is propogated up the networking stack.

Now when I tried pinging my VM from the host machine, the request timed out and BPF prints below trace for each ICMP request arriving at the primary interface.

```
b'          <idle>-0       [000] d.s.  6864.619220: bpf_trace_printk: blocking icmp packet'
b'          <idle>-0       [000] d.s.  6869.432444: bpf_trace_printk: blocking icmp packet'
b'          <idle>-0       [000] d.s.  6874.430802: bpf_trace_printk: blocking icmp packet'
b'           gedit-2605    [000] d.s.  6879.430771: bpf_trace_printk: blocking icmp packet'
```

Success. There are a plethora of other applications of BPF like load balancers, HTTP monitoring, DNS request monitoring. I have included the links to these references below.

References
1. [https://github.com/zoidbergwill/awesome-ebpf](https://github.com/zoidbergwill/awesome-ebpf)
2. [https://github.com/iovisor/bcc](https://github.com/iovisor/bcc)
3. [https://github.com/lizrice/ebpf-beginners](https://github.com/lizrice/ebpf-beginners)
4. [https://www.youtube.com/watch?v=0p987hCplbk](https://www.youtube.com/watch?v=0p987hCplbk)
5. [https://github.com/iovisor/bcc/blob/master/docs/reference_guide.md](https://github.com/iovisor/bcc/blob/master/docs/reference_guide.md)