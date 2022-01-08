---
layout: post
title: RAMoops panic logger on Raspberry Pi
---
There is a neat feature in Linux and I decided to write it up before I forget it. Has your system ever gone into a reset and you wished that the kernel panic logs were somehow preserved?

We can ensure logging of kernel OOPS situations by enabling the ramoops feature present in the Linux kernel. The access to these logs is provided using the Pstore filesystem interface. I will demonstrate the setup for a Raspberry Pi board.

### Enable Pstore in the kernel ###
We will need to build the kernel with Pstore option enable. Modify the .config file to add the following option.
```
CONFIG_PSTORE=y
CONFIG_PSTORE_RAM=y
```
While building the kernel, you will be asked to select the compression method for the logs, ftrace support and some more extras, go ahead and provide Yes to all of them.
Once the kernel is built, rename the zImage file as kernel7 and copy it into the boot partition of the RPi's SD card
### Add a Device Tree Overlay ###
In order for ramoops to work, we need to reserve a region of the RAM area for the logs to be stored. This is done by adding a device tree overlay which will be picked by the kernel during boot and used along with the main device tree (DTB) file.

```
/dts-v1/;
/plugin/;

/ {
	compatible = "brcm,bcm2710";

	fragment@0 {
		target-path = "/";
		__overlay__ {
			reserved-memory {
				#address-cells = <1>;
				#size-cells = <1>;
				ranges;

				ramoops: ramoops@039900000 {
					compatible = "ramoops";
					reg = <0x39900000 0x100000>; 
					record-size = <0x4000>; 
					console-size = <0>; /* disabled by default */
				};
			};
		};
	};

	__overrides__ {
		total-size = <&ramoops>,"reg:4";
		record-size = <&ramoops>,"record-size:0";
		console-size = <&ramoops>,"console-size:0";
	};
};
```
The Pi that I am using comes with 1GB of RAM, so the upper limit on the physical address is `0x40000000`. I chose to create the ramoops area from `0x39900000` for a length of `0x100000`.

Save the above contents as ramoops_overlay.dts and compile it using the Device Tree Compiler present at scripts/dtc/dtc in the kernel sources.
```
$ ./scripts/dtc/dtc -@ -I dts -O dtb -o ramoops.dtbo ramoops-overlay.dts
```
Copy the ramoops.dtbo file into the `overlays` folder of the boot partition of the SD card.

Add `dtoverlay=ramoops` in the `config.txt` file and reboot the Pi.

### Create a Panic ###
Check if ramoops and pstore was enabled using the dmesg logs
```
$ dmesg

[ ... ]
[    0.085368] pstore: using zlib compression
[    0.085382] pstore: Registered ramoops as persistent store backend
[    0.085389] ramoops: attached 0x100000@0x39900000, ecc: 0/0
[ ... ]
```
Test the feature by creating a kernel panic
```
$ sudo sh -c "echo 10 > /proc/sys/kernel/panic"
$ sudo sh -c "echo c > /proc/sysrq-trigger"
```
Now the system will reboot. You will find the logs at `/sys/fs/pstore`. It contains details of the register state, stack memory and function backtrace.
```
$ ls -l /sys/fs/pstore
-r--r--r-- 1 root root 21188 Mar  1 16:49 dmesg-ramoops-0
-r--r--r-- 1 root root 25952 Mar  1 16:49 dmesg-ramoops-1
```

#### References ####
1. [https://www.kernel.org/doc/html/v4.12/admin-guide/ramoops.html](https://www.kernel.org/doc/html/v4.12/admin-guide/ramoops.html)
2. [https://www.kernel.org/doc/Documentation/ABI/testing/pstore](https://www.kernel.org/doc/Documentation/ABI/testing/pstore)
3. [Raspberry Pi forums user : notro](https://www.raspberrypi.org/forums/viewtopic.php?t=199047)
