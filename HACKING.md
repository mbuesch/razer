# Sniffing the USB protocol

When adding support for a new mouse, or when debugging issues, it can be usefull to capture the original protocol with Razer official configuration software.

## Requirements

No matter the method, the basic requirements are:

- a Windows machine with [Razer Synapse](http://www.razerzone.com/synapse/) installed (note: you'll need to register)
- another mouse: to reduce protocol chatter by avoiding moving the mouse spied on

## Using USBPcap

You can sniff the USB protocol on Windows using [USBPcap](http://desowin.org/usbpcap/index.html). Take a look at the [illustrated tour](http://desowin.org/usbpcap/tour.html) for more information.

## Using a guest VirtualBox VM and usbmon.

1. install Windows: you can get an evaluation copy (usable for 90 days) of [Windows 7 Entreprise](http://www.microsoft.com/en-us/evalcenter/evaluate-windows-7-enterprise) (note: you'll need to register)

2. find your mouse USB bus and device numbers:

 ```shell
> lsusb | grep '\<ID 1532:'
Bus 005 Device 010: ID 1532:0040 Razer USA, Ltd
> bus=5 dev=10
 ```

3. configure VM access to the host USB: see the relevant [USB support](http://www.virtualbox.org/manual/ch03.html#idp54668304) section in the [VirtualBox manual](https://www.virtualbox.org/manual)

4. load the usbmon module:

 ```shell
> sudo modprobe usbmon
 ```

5. monitor the bus, filtering for device specific messages:

 ```shell
> sudo cat /sys/kernel/debug/usb/usbmon/${bus}u | awk "{ if (\$4 ~ /:0*${bus}:0*${dev}:[0-9]+$/) print \$0 }"
 ```

See the [usbmon documentation](https://www.kernel.org/doc/Documentation/usb/usbmon.txt) for the details of the output format.
