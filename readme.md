# UEFI Bootloader

A modern, no-flicker UEFI boot manager written in C.  Boots Windows, any
GRUB-based Linux distro, and Linux kernels directly via the EFI stub.


## File layout

```
.
├── Makefile
├── bootloader.conf.example      ← copy to image/EFI/BOOT/bootloader.conf
├── include/
│   ├── ui.h                     ← no-flicker rendering API
│   ├── config.h                 ← config file parser API
│   └── boot.h                   ← OS boot execution API
└── src/
    ├── main.c                   ← efi_main(), event loop
    ├── ui.c                     ← all screen drawing
    ├── config.c                 ← parses bootloader.conf
    └── boot.c                   ← launches EFI / Linux stub / chainload
```

## Dependencies

```bash
# Arch Linux
sudo pacman -S gnu-efi qemu ovmf

# Debian / Ubuntu
sudo apt install gnu-efi ovmf qemu-system-x86

# Fedora
sudo dnf install gnu-efi gnu-efi-devel edk2-ovmf qemu-system-x86
```

## Building and running

```bash
# Build the .efi file
make

# Build AND launch in QEMU immediately
make run

# Rebuild after editing config only (no recompile needed)
make image && make run

# Clean
make clean
```

## Configuration

Copy `bootloader.conf.example` to `image/EFI/BOOT/bootloader.conf` and edit
it to match your actual partition layout.

```ini
timeout = 5       # seconds before auto-booting default entry (0 = wait forever)
default = 0       # 0-based index of the entry to auto-boot

[entry]
label = Arch Linux
type  = linux                        # linux | efi | chain
path  = \vmlinuz-linux
initrd = \initramfs-linux.img
args  = root=/dev/sda2 rw quiet
default = true                       # overrides global `default =`

[entry]
label = Windows 11
type  = efi
path  = \EFI\Microsoft\Boot\bootmgfw.efi
```

The config is re-read from disk at runtime when you press **F5** — no reboot
needed.

## Key bindings

| Key          | Action                        |
|--------------|-------------------------------|
| ↑ / ↓        | Move selection                |
| Home / End   | Jump to first / last entry    |
| Enter        | Boot selected entry           |
| F5           | Reload config from disk       |
| Any key      | Cancel auto-boot countdown    |

## Testing your Arch kernel in QEMU

```bash
# Copy your kernel and initrd into the image directory
cp /boot/vmlinuz-linux   image/
cp /boot/initramfs-linux.img image/

# Edit image/EFI/BOOT/bootloader.conf – set the root= to match your real disk
# Then:
make run
```

## Installing to a real EFI System Partition

```bash
# Mount your ESP (usually /dev/sda1 or /dev/nvme0n1p1)
sudo mount /dev/sda1 /mnt/efi

# Copy the binary
sudo mkdir -p /mnt/efi/EFI/BOOT
sudo cp bootloader.efi /mnt/efi/EFI/BOOT/BOOTX64.EFI

# Copy the config
sudo cp bootloader.conf.example /mnt/efi/EFI/BOOT/bootloader.conf
# Edit /mnt/efi/EFI/BOOT/bootloader.conf for your system

sudo umount /mnt/efi
```

Then set the boot order in your UEFI firmware settings to put
`BOOTX64.EFI` first.