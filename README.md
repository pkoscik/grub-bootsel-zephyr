# Zephyr based GRUB boot selector switch

This repository contains a [Zephyr RTOS](https://github.com/zephyrproject-rtos/zephyr) based application to switch between states stored on a file system (FAT), controlled via a button, with visual feedback using LEDs.

## Quickstart

Ensure that the Zephyr SDK and `west` tool are installed. For setup instructions, refer to the [Zephyr Getting Started Guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html).

### Preparing hardware

Connect the Raspberry Pi Pico (W) to the LEDs, button, and optionally an FTDI breakout board for UART output:

```
                ┌────────────────┐
          RX────┤1               │
          TX────┤2               │
         GND────┤3               │
                │                │
                │                │
                │                │
                │    RPI PICO    │
                │                │
                │                │
                │                │
                │                │
GND──LED──10kΩ──┤16              │
GND──LED──10kΩ──┤17              │
                │                │
                │                │
      GND──BTN──┤20              │
                └────────────────┘
```

### Cloning the project

```sh
west init -m https://github.com/pkoscik/grub-bootsel-zephyr bootsel-prj
cd bootsel-prj
west update
```

### Building and flashing the Zephyr application

```sh
west build -b rpi_pico/rp2040/w app
```

### Flashing the Board

- Put the board in bootloader mode by holding the `BOOTSEL` button while plugging it into your computer.
- Copy the `zephyr.uf2` file to the mounted drive.

### Usage

Press the button to toggle the contents of the `BOOTSEL` file between `0` and `1`. The state change will be reflected by the indicator LEDs.

> [!NOTE]  
> Operating systems may cache the contents of the filesystem, which can prevent updates to the `BOOTSEL` file from being immediately visible. This is not an issue when used with GRUB. To verify that toggling works as expected, unmount and remount the USB device after pressing the button.

### UART output

The (optional) UART output can be used for diagnosing issues with the board during initialization and operation. Below is an example of the output from a properly functioning board:

```
*** Booting Zephyr OS build v4.0.0 ***
[00:00:00.001,000] <inf> flashdisk: Initialize device NAND
[00:00:00.001,000] <inf> flashdisk: offset 100000, sector size 512, page size 4096, volume size 1048576
[00:00:00.002,000] <inf> usb_msc: Sect Count 2048
[00:00:00.002,000] <inf> usb_msc: Memory Size 1048576
[00:00:00.002,000] <inf> bootsel: Area 0 at 0x100000 on flash-controller@18000000 for 1048576 bytes
[00:00:00.003,000] <inf> bootsel: Set up button at gpio@40014000 pin 15
[00:00:00.003,000] <inf> bootsel: Set up LED at gpio@40014000 pin 12
[00:00:00.003,000] <inf> bootsel: Set up LED at gpio@40014000 pin 13
[00:00:00.003,000] <inf> bootsel: GPIO configured successfully
[00:00:00.004,000] <inf> bootsel: Boot toggle state: 1
[00:00:00.005,000] <inf> bootsel: The device is put in USB mass storage mode.
```

> [!NOTE]  
> On the first boot, you may encounter an error message like this:

```
[00:00:00.003,000] <err> fs: file open error (-2)
[00:00:00.004,000] <inf> bootsel: Error -2: file '/NAND:/BOOTSEL' not found
[00:00:00.004,000] <wrn> bootsel: Failed to read BOOTSEL! Assuming toggle_state: false
```

This is normal and occurs because the BOOTSEL file does not yet exist on the filesystem. The error will not appear on subsequent boots after the file has been created.

## Hardware compability

This project has been tested with the Raspberry Pi Pico W, though it should also work properly on the non-W version.

## Special Thanks

Special thanks to [stecman/hw-boot-selection](https://github.com/stecman/hw-boot-selection) for inspiring the development of this project!
