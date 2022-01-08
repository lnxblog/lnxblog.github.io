---
layout: post
title: Understanding ARM Trusted Firmware using QEMU
---
For many of us working in the embedded world with ARM devices, it is not uncommon to have come across the company’s marketing terms on security - “TrustZone”, “Trusted Execution Environment”, “Trusted World”.
I wanted to understand this technology and more importantly OP-TEE, an OS running in the trusted zone specifically designed to provide secure services.
But before that, we need to understand the ARM trusted firmware (ATF). Here I will describe the ATF for the ARM v8-A profile.

## Intro ##
So the definition of ARM TF:

> “ARM Trusted Firmware is a Reference implementation of secure world software for ARMv8-A, including Exception Level 3 (EL3) software.”  

My understanding is that ARM has developed software to standardize interactions between the non-secure and the secure world and made it open source.
The source files for ATF are available in [_github_](https://github.com/ARM-software/arm-trusted-firmware)
The components part of the ATF are `BL1`, `BL2` and `BL31`.
A typical system will consist of five components:
1. Boot Loader stage 1 (BL1) *AP Trusted ROM*
2. Boot Loader stage 2 (BL2) *Trusted Boot Firmware*
3. Boot Loader stage 3-1 (BL31) *EL3 Runtime Software*
4. Boot Loader stage 3-2 (BL32) *Secure-EL1 Payload* (optional)
5. Boot Loader stage 3-3 (BL33) *Non-trusted Firmware*

## Boot chain procedure ##
ARM has introduced four exception levels for their 64 bit v8 cores with privilege levels ranked as:

> **EL0 < EL1 < EL2 <EL3**

 
EL0 runs at the lowest exception level
EL3 runs at the highest exception level

The boot process can be diagrammatically shown as below

<figure><img src="../../../assets/images/atf.png" width="500px">
<figcaption style="text-align:center"><i>&copy; Virtual Open Systems</i></figcaption></figure>

The first component to begin execution during a cold boot is the BL1 which runs in EL3 . BL1 loads BL2 which runs in secure EL1. 

BL2 performs loading of BL31, BL32 and BL33 and hands the load address of BL31 to BL1.
Since BL31 runs in EL3 it must be invoked by another BL3 component like BL1.

BL31 then initializes the BL32 which is the OPTEE OS in this case and runs in secure EL1. It also triggers execution of BL33 which can be an OS or hypervisor. BL33 runs in EL2 in case of a hypervisor and EL1 in case of a Normal OS like Linux. 
BL31 is responsible for handling secure monitor call for interactions between secure and non-secure world. It also handles PSCI (Power System Control Interface) operations.
> More details of the implementation can be found in firmware-design.rst file in the docs/design path of the ATF sources

## Running on QEMU ##
In order to see how this works, I decided to test it on QEMU for arm64.
For BL32 I used the Optee OS as a secure payload
For BL33 I decided to use U-boot to boot a Linux kernel
In ATF documentation for [QEMU](https://github.com/ARM-software/arm-trusted-firmware/blob/master/docs/plat/qemu.rst) boot, it was suggested to use QEMU_EFI.fd for BL33 to boot a Linux Image but I wasn’t able to make it work or debug it, so decided to use U-boot instead.

You will need sources for the below components
1. [ATF](https://github.com/ARM-software/arm-trusted-firmware)
2. [OPTEE-OS](https://github.com/OP-TEE/optee_os)
3. [U-boot](https://github.com/u-boot/u-boot)
4. [QEMU](https://download.qemu.org/) (not needed if your version >= 2.6)
5. [Linux](https://github.com/torvalds/linux)
6. [BusyBox](https://www.busybox.net/downloads/)
7. [Optee build rules](https://github.com/OP-TEE/build)

### Building ATF and OPTEE-OS ###
I used the build rules from qemu_v8.mk of the optee repos for compiling atf and optee.
[https://github.com/OP-TEE/build](https://github.com/OP-TEE/build)

These rules create softlinks to the compiled binaries making it convenient to use.
```
make arm-tf
make optee-os
```

### Building U-boot ###
U-boot needs to be compiled for arm64 qemu config.
You will need to modify the base address of the text segment else execution of u-boot will fail. ATF seems to set the entrypoint of BL33 at `0x60000000`. We can modify it but I have not tried it yet.

Add the below line to `configs/qemu_arm64_defconfig`
```
CONFIG_SYS_TEXT_BASE=0x60000000
```
Next compile u-boot
```
export CROSS_COMPILE=aarch64-linux-gnu-
make qemu_arm64_defconfig
make
```

### Building QEMU ###
Again use the build rules from `qemu_v8.mk` of optee build rules
```
make qemu
```
### Building Linux ###
Build the default for arm64 config
```
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
make Image
```
### Building BusyBox ###
BusyBox contains necessary binaries for running userspace
```
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- install
```
The compiled binaries will be present in the `_install` directory. These binaries should be available in an image file formatted with a standard filesystem like `ext3` or `ext4`.
```
dd if=/dev/zero of=rootfs.img bs=1M count 256
mkfs.ext4 rootfs.img
mkdir mountpoint
sudo mount rootfs.img ./mountpoint
```
Copy the binary contents of _install directory into the mounted image. These will be dynamically linked to the toolchain libraries, so copy the libraries into a lib folder on the mounted image.
### Let’s Boot ###
 
Here u-boot is BL33 which is used to load and boot the linux kernel. In u-boot I used the `booti` command to start the kernel. But u-boot requires a DTB in order to get the device layout. 
We have compiled the default config in the Linux kernel and this does not correspond to any specific machine.
On the qemu we will be using the `virt` machine, so we require its DTB.
Thanks to [*Simon the coder*](http://simonthecoder.blogspot.com/2018/12/get-qemu-virt-machine-dts.html), I was able to get a dump of the `virt` machine.
```
qemu-system-aarch64 -machine virt,dumpdtb=/tmp/virt.dtb
dtc -I dtb -O dts /tmp/virt.dtb >/tmp/virt.dts 
```
With everything in place we can now run QEMU. I decided to use TFTP  to load the kernel and dtb in u-boot since I had the setup ready from a previous experiment. You can read the setup details [*here*](https://lnxblog.github.io/2019/02/17/uboot-arm-qemu.html).

```
qemu-system-aarch64 -machine virt,secure=on -cpu cortex-a57 -smp 2 -m 1024 -bios bl1.bin  -serial stdio  -semihosting-config enable,target=native -device virtio-net-device,netdev=user0 -netdev user,id=user0 -s -device virtio-blk-device,drive=image -drive if=none,id=image,file=rootfs.img

-smp 2 - 2 (virtual) cores
-m 1024 - 1024MB of system memory
-M virt,secure=on - emulate a generic QEMU ARM machine with secure features
-cpu cortex-a57 - the CPU model to emulate
-bios bl1.bin - the BIOS firmware file to use
-serial stdio - Redirect the virtual serial port to host character device
-semihosting-config enable,target=native - Enables transfer of components following bl1.bin: bl31.bin, bl32.bin, bl33.bin
-device virtio-net-device,netdev=user0 - create a Virtio network device called "user0"
-netdev user,id=user0 - create a user mode network stack using device "user0"
-device virtio-blk-device,drive=image - create a Virtio block device called "image"
-drive if=none,id=image,file=rootfs.img - create a drive using the "image" device and our rootfs.img image
```

The kernel boot-up from U-boot was hung. Thankfully one of the neat features of QEMU is it provides option (-s) to start gdbserver which helps debug the binaries running.

I was able to observe that the failure was occurring in a PSCI operation.

**arch/arm64/kernel/setup.c**
```
void __init setup_arch(char **cmdline_p)
{
…
	if (acpi_disabled)
		psci_dt_init()
	else
		psci_acpi_init()
…
}
```

**drivers/firmware/psci.c**
```
...
static const struct of_device_id psci_of_match[] __initconst = {
	{ .compatible = "arm,psci",	.data = psci_0_1_init},
	{ .compatible = "arm,psci-0.2",	.data = psci_0_2_init},
	{ .compatible = "arm,psci-1.0",	.data = psci_0_2_init},
	{},
};

int __init psci_dt_init(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	psci_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, psci_of_match, &matched_np);

	if (!np)
		return -ENODEV;

	init_fn = (psci_initcall_t)matched_np->data;
	return init_fn(np);
}

static int __init psci_0_2_init(struct device_node *np)
{
	int err;

	err = get_set_conduit_method(np);
	if (err)
		goto out_put_node;
	err = psci_probe();

out_put_node:
	of_node_put(np);
	return err;
}

static int get_set_conduit_method(struct device_node *np)
{
	const char *method;

	pr_info("probing for conduit method from DT.\n");

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		return -ENXIO;
	}

	if (!strcmp("hvc", method)) {
		invoke_psci_fn = __invoke_psci_fn_hvc;
	} else if (!strcmp("smc", method)) {
		invoke_psci_fn = __invoke_psci_fn_smc;
	} else {
		pr_warn("invalid \"method\" property: %s\n", method);
		return -EINVAL;
	}
	return 0;
}
...
```
**virt.dts**
```
...
	psci {
		migrate = <0x84000005>;
		cpu_on = <0x84000003>;
		cpu_off = <0x84000002>;
		cpu_suspend = <0x84000001>;
		method = "hvc";
		compatible = "arm,psci-0.2", "arm,psci";
	};
...
```
The function call graph is:
1. get_set_conduit_method
2. psci_0_2_init
3. psci_dt_init
4. setup_arch

The `psci_dt_init` uses the device tree's compaitable strings to determine the correct psci init method. The `psci_0_2_init` method reads the device tree to determine if 'hvc' or 'smc' should be used to perform psci operations.
The reason for this is Linux cannot directly perform these operations as it requires execution in EL3 mode. 
I could see from the device tree that the psci method is specified as `hvc`. But I am not running any hypervisor rather a bare-metal Linux. Changing it to `smc` and recompiling the `DTB`, the kernel was able to boot up.

To summarize the list of commands i followed to boot the machine.
```
$ qemu-system-aarch64 -machine virt,secure=on -cpu cortex-a57 -smp 2 -m 1024 -bios bl1.bin  -serial stdio  -semihosting-config enable,target=native -device virtio-net-device,netdev=user0 -netdev user,id=user0 -s -device virtio-blk-device,drive=image -drive if=none,id=image,file=rootfs.img

U-boot console $ dhcp
U-boot console $ setenv serverip 127.0.0.1
U-boot console $ tftp 0x40000000 Image
U-boot console $ tftp 0x50000000 virt.dtb
U-boot console $ setenv bootargs "console=ttyAMA0 rw root=/dev/vda"
U-boot console $ booti 0x40000000 - 0x50000000
```
If everything goes right, you should see logs like below
```
NOTICE:  Booting Trusted Firmware
NOTICE:  BL1: v2.3():
NOTICE:  BL1: Built : 21:50:57, Jul 27 2020
WARNING: Firmware Image Package header check failed.
NOTICE:  Loading image id=1 at address 0xe01b000
NOTICE:  BL1: Booting BL2
NOTICE:  reached hereNOTICE:  Entry point address = 0xe01b000
NOTICE:  BL2: v2.3():
NOTICE:  BL2: Built : 10:37:11, Jul 28 2020
NOTICE:  BL2: Loading image id 3 address e040000 size 0
WARNING: Firmware Image Package header check failed.
NOTICE:  Loading image id=3 at address 0xe040000
NOTICE:  BL2: Loading image id 4 address e100000 size 0
WARNING: Firmware Image Package header check failed.
NOTICE:  Loading image id=4 at address 0xe100000
NOTICE:  BL2: Loading image id 21 address e100000 size 407760
WARNING: Firmware Image Package header check failed.
NOTICE:  Loading image id=21 at address 0xe100000
NOTICE:  BL2: Loading image id 5 address 60000000 size 0
WARNING: Firmware Image Package header check failed.
NOTICE:  Loading image id=5 at address 0x60000000
NOTICE:  BL1: Booting BL31
NOTICE:  Entry point address = 0xe040000
NOTICE:  BL31: v2.3():
NOTICE:  BL31: Built : 21:50:57, Jul 27 2020
I/TC: OP-TEE version: Unknown (gcc version 7.5.0 (Linaro GCC 7.5-2019.12)) #3 Sun Jul 26 15:41:40 UTC 2020 aarch64
I/TC: Primary CPU initializing
D/TC:0 0 paged_init_primary:1180 Executing at offset 0 with virtual load address 0xe100000
D/TC:0 0 call_initcalls:21 level 1 register_time_source()
D/TC:0 0 call_initcalls:21 level 1 teecore_init_pub_ram()
D/TC:0 0 call_initcalls:21 level 3 check_ta_store()
D/TC:0 0 check_ta_store:636 TA store: "Secure Storage TA"
D/TC:0 0 check_ta_store:636 TA store: "REE"
D/TC:0 0 call_initcalls:21 level 3 init_user_ta()
D/TC:0 0 call_initcalls:21 level 3 verify_pseudo_tas_conformance()
D/TC:0 0 call_initcalls:21 level 3 mobj_mapped_shm_init()
D/TC:0 0 mobj_mapped_shm_init:447 Shared memory address range: fa00000, 11a00000
D/TC:0 0 call_initcalls:21 level 3 tee_cryp_init()
D/TC:0 0 call_initcalls:21 level 4 tee_fs_init_key_manager()
D/TC:0 0 call_initcalls:21 level 6 mobj_init()
D/TC:0 0 call_initcalls:21 level 6 default_mobj_init()
D/TC:0 0 call_finalcalls:40 level 1 release_external_dt()
I/TC: Primary CPU switching to normal world boot
NOTICE:  BL31: Preparing for EL3 exit to normal world
NOTICE:  Entry point address = 0x60000000


U-Boot 2019.01 (Jul 27 2020 - 22:37:04 +0530)

DRAM:  1 GiB
Flash: ## Unknown flash on Bank 2 - Size = 0x00000000 = 0 MB
64 MiB
*** Warning - bad CRC, using default environment

In:    pl011@9000000
Out:   pl011@9000000
Err:   pl011@9000000
Net:   
Warning: virtio-net#31 using MAC address from ROM
eth0: virtio-net#31
Hit any key to stop autoboot:  0 
=> dhcp
BOOTP broadcast 1
DHCP client bound to address 10.0.2.15 (3 ms)
*** Warning: no boot file name; using '0A00020F.img'
Using virtio-net#31 device
TFTP from server 10.0.2.2; our IP address is 10.0.2.15
Filename '0A00020F.img'.
Load address: 0x40200000
Loading: *
TFTP error: 'Access violation' (2)
Not retrying...
=> setenv serverip 127.0.0.1
=> tftp 0x40000000 Image_bk2
Using virtio-net#31 device
TFTP from server 127.0.0.1; our IP address is 10.0.2.15; sending through gateway 10.0.2.2
Filename 'Image_bk2'.
Load address: 0x40000000
Loading: #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 #################################################################
	 ##############################
	 2.3 MiB/s
done
Bytes transferred = 16652800 (fe1a00 hex)
=> tftp 0x50000000 virt.dtb 
Using virtio-net#31 device
TFTP from server 127.0.0.1; our IP address is 10.0.2.15; sending through gateway 10.0.2.2
Filename 'virt.dtb'.
Load address: 0x50000000
Loading: #
	 3.2 MiB/s
done
Bytes transferred = 6725 (1a45 hex)
=> setenv bootargs "console=ttyAMA0,115200 rw root=/dev/vda rootfstype=ext4"
=> booti 0x40000000 - 0x50000000
## Flattened Device Tree blob at 50000000
   Booting using the fdt blob at 0x50000000
   Loading Device Tree to 000000007eef5000, end 000000007eef9a44 ... OK

Starting kernel ...

[    0.000000] Booting Linux on physical CPU 0x0
[    0.000000] Linux version 4.14.52 (hema@MyRig) (gcc version 7.5.0 (Linaro GCC 7.5-2019.12)) #5 SMP PREEMPT Mon Jul 27 21:37:46 IST 2020
[    0.000000] Boot CPU: AArch64 Processor [411fd070]
[    0.000000] Machine model: linux,dummy-virt
[    0.000000] efi: Getting EFI parameters from FDT:
[    0.000000] efi: UEFI not found.
[    0.000000] cma: Reserved 16 MiB at 0x000000007f000000
[    0.000000] NUMA: No NUMA configuration found
[    0.000000] NUMA: Faking a node at [mem 0x0000000000000000-0x000000007fffffff]
[    0.000000] NUMA: NODE_DATA [mem 0x7efeae80-0x7efec97f]
...
...
[    6.279069] EXT4-fs (vda): recovery complete
[    6.286949] EXT4-fs (vda): mounted filesystem with ordered data mode. Opts: (null)
[    6.289445] VFS: Mounted root (ext4 filesystem) on device 254:0.
[    6.295841] devtmpfs: error mounting -2
[    6.486347] Freeing unused kernel memory: 1152K

can't run '/etc/init.d/rcS': No such file or directory

/ # ls
 bin  lib  linuxrc  lost+found  proc  sbin  usr
 / #
 
```
The entire setup might seem tedious but it is a one time effort to help us explore ATF and OP-TEE better.

In my future articles, I will try to cover some of my learnings with the OP-TEE OS.
Feel free to try these steps and let me know how it went in the comments.







