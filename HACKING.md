Razercfg - Sniffing the USB protocol
====================================

When adding support for a new mouse, or when debugging issues, it can be usefull to capture the original protocol with Razer official configuration software.

Requirements
------------

No matter the method, the basic requirements are:

* a Windows machine with [Razer Synapse](http://www.razerzone.com/synapse/) installed (note: you'll need to register)
* another mouse: to reduce protocol chatter by avoiding moving the mouse spied on

Using USBPcap or Wireshark
--------------------------

You can sniff the USB protocol on Windows using [USBPcap](http://desowin.org/usbpcap/index.html). Take a look at the [illustrated tour](http://desowin.org/usbpcap/tour.html) for more information.
Altenatively you can use [Wireshark](https://www.wireshark.org/).

Using a guest VirtualBox VM and usbmon or QEMU and usbmon
---------------------------------------------------------

1. Install Windows: you can get an evaluation copy (usable for 90 days) of [Windows 8/8.1/10](https://www.microsoft.com/en-us/evalcenter/evaluate-windows-8-1-enterprise) (note: you'll need to register)

2. Find your mouse USB bus and device numbers:

<pre>
$ lsusb | grep 'ID 1532'
Bus 005 Device 010: ID 1532:0040 Razer USA, Ltd
$ bus=5 dev=10
</pre>

3. configure VM access to the host USB: see the relevant [USB support](http://www.virtualbox.org/manual/ch03.html#idp54668304) section in the [VirtualBox manual](https://www.virtualbox.org/manual)

4. load the usbmon module:

<pre>
sudo modprobe usbmon
</pre>

5. monitor the bus, filtering for device specific messages:

<pre>
sudo cat /sys/kernel/debug/usb/usbmon/${bus}u | awk "{ if (\$4 ~ /:0*${bus}:0*${dev}:[0-9]+$/) print \$0 }"
</pre>

See the [usbmon documentation](https://www.kernel.org/doc/Documentation/usb/usbmon.txt) for the details of the output format.

Alternatively to VirtualBox VM, you may use [QEMU](http://wiki.qemu.org/Main_Page).
