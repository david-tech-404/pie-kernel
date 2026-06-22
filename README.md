## Background



The Samsung Wave S8500, released in 2010, ran Samsung's Bada OS — a mobile

operating system built on a Linux kernel running as a guest under the OKL4

microkernel (Open Kernel Labs), with a custom application framework called

OSP (Open Service Platform) and a Windows CE-style window server (GWES).



Samsung never published the source code for the modified Linux 2.6.32 kernel

used in Bada OS. This project, Pie Kernel, is an attempt to build a

functionally compatible Linux kernel for the same hardware, using mainline

Linux 3.4.99 as a base, so that the original OSP framework libraries

(extracted from the Bada 1.2 firmware) can run on top of it.



## Why Linux 3.4.99?



- Released in 2012, close to the Bada 1.2 / 2.0 era (2010-2011)

- Already includes MSM7x27 board support (CONFIG\_ARCH\_MSM7X27)

- Includes mainline drivers for WM8994 audio codec and BCM4329 WiFi

&#x20; (brcmfmac), both used in the Wave S8500

- ARM EABI5 compatible — matches the ABI of the extracted Bada .so libraries

&#x20; (FOsp.so, mosp.so, etc. are all "ELF 32-bit LSB shared object, ARM, EABI5")



## Hardware reference (Samsung GT-S8500 Wave)



- SoC: Samsung S5PC110 / Qualcomm MSM7227, ARM Cortex-A8 @ 800MHz

- RAM: 256MB

- Display: 3.3" AMOLED, 480x800 (WVGA)

- Camera: Samsung CE147, 5MP

- WiFi: Broadcom BCM4329 (802.11 b/g/n)

- Audio codec: Wolfson WM8994

- Touchscreen: Synaptics capacitive, I2C



## Open questions / TODO



- The original OKL4 microkernel layer is not reproduced here. Pie Kernel

&#x20; runs Linux directly on hardware/QEMU, skipping the OKL4 hypervisor layer

&#x20; present in the original Bada architecture. This should still be ABI

&#x20; compatible from the OSP framework's point of view, since OSP runs as

&#x20; userspace Linux processes either way.

- The `.rc1` resource file (Rsrc\_S8500\_Open\_Europe\_Common.rc1) uses an

&#x20; encrypted Samsung-proprietary format (QMD) and could not yet be decoded.

&#x20; System fonts, themes, and some icons may be missing until this is solved.

- Camera, display, and touchscreen drivers in this repo are written from

&#x20; scratch based on the original firmware blobs and register-level

&#x20; documentation patterns common to similar Samsung devices of the era.

&#x20; They have not been hardware-tested.

