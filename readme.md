# ZhrBea Boot Manager

A custom UEFI boot manager written from scratch in C. No GRUB, no systemd-boot — just a clean, fast, minimal bootloader with a modern graphical UI running directly on the UEFI framebuffer.

Built by zhrysn.

---

## Features

- **GOP graphics** — pixel-level framebuffer rendering, no text console
- **Animated splash screen** — smooth fade-in on boot
- **Clean minimal dark UI** — sharp panel design with purple accent colour
- **Pre-boot health checks** — scans kernel, initramfs, and config readability before showing the menu. No other bootloader does this
- **OS auto-detection** — scans the EFI partition for Windows, Ubuntu, Fedora, Arch kernels and more without manual config
- **Kernel version display** — reads version string directly from vmlinuz binary (e.g. `Arch Linux (6.6.8-arch1-1)`)
- **Boot history** — remembers last booted entry in UEFI NVRAM, auto-selects it next boot
- **Zero flicker** — only changed rows are repainted
- **Live config reload** — F5 rescans entries without rebooting
- **Countdown timer** — auto-boots default with a progress bar

---

## Project Structure

```
.
├── Makefile
├── bootloader.conf.example
├── include/
│   ├── gop.h          pixel framebuffer API + colours
│   ├── ui.h           menu and chrome rendering API
│   ├── config.h       config parser
│   ├── boot.h         OS launch logic
│   ├── scanner.h      EFI partition auto-scanner
│   ├── history.h      NVRAM boot history
│   ├── health.h       pre-boot diagnostics
│   └── splash.h       boot animation
└── src/
    ├── main.c         entry point, event loop
    ├── ui.c           all UI rendering
    ├── gop.c          GOP framebuffer + bitmap font
    ├── splash.c       boot animation + background
    ├── config.c       bootloader.conf parser
    ├── boot.c         boots Linux stubs, EFI binaries, chainload
    ├── scanner.c      auto-detects OSes on EFI partition
    ├── history.c      NVRAM last-boot storage
    └── health.c       pre-boot health checks
```

---

## Dependencies

```bash
# Arch Linux
sudo pacman -S gnu-efi qemu ovmf

# Debian / Ubuntu
sudo apt install gnu-efi ovmf qemu-system-x86

# Fedora
sudo dnf install gnu-efi gnu-efi-devel edk2-ovmf qemu-system-x86
```

---

## Build & Run

```bash
make clean && make run    # build + launch in QEMU
make                      # build only
make clean                # clean artifacts
```

---

## Configuration

Copy `bootloader.conf.example` to your EFI partition:

```ini
timeout = 5       # seconds before auto-boot (0 = wait forever)
default = 0       # index of default entry

[entry]
label  = Arch Linux
type   = linux
path   = \vmlinuz-linux
initrd = \initramfs-linux.img
args   = root=/dev/sda2 rw quiet loglevel=3
default = true

[entry]
label = Windows 11
type  = efi
path  = \EFI\Microsoft\Boot\bootmgfw.efi
```

Supported types: `linux`, `efi`, `chain`

Press **F5** to reload config live without rebooting.

---

## Key Bindings

| Key        | Action                         |
|------------|--------------------------------|
| ↑ / ↓      | Move selection                 |
| Home / End | Jump to first / last entry     |
| Enter      | Boot selected entry            |
| F5         | Rescan entries + reload config |
| Any key    | Cancel auto-boot countdown     |

---

## Installing to Real Hardware

```bash
sudo mount /dev/sda1 /mnt

# Backup existing bootloader
sudo cp /mnt/EFI/arch-limine/BOOTX64.EFI /mnt/EFI/arch-limine/BOOTX64.EFI.backup

# Install ZhrBea
sudo cp bootloader.efi /mnt/EFI/arch-limine/BOOTX64.EFI
sudo cp image/EFI/BOOT/bootloader.conf /mnt/EFI/arch-limine/bootloader.conf

sudo umount /mnt && reboot
```

**Recovery:**
```bash
sudo mount /dev/sda1 /mnt
sudo cp /mnt/EFI/arch-limine/BOOTX64.EFI.backup /mnt/EFI/arch-limine/BOOTX64.EFI
sudo umount /mnt && reboot
```

---

## How the Health Check Works

Before the menu appears, ZhrBea silently runs:

1. EFI filesystem readable check
2. Kernel file exists + file size
3. Initramfs exists + file size
4. Config file found vs built-in defaults

Results show as coloured dots at the bottom of the panel. Green = OK, yellow = warning, red = missing. You see this before pressing Enter — no other bootloader does this.

---

## How Boot History Works

Last-booted entry index is stored as a UEFI NVRAM variable `BootloaderLastEntry` via `gRT->SetVariable`. Persists in firmware NVRAM across reboots exactly like UEFI boot order. Read back on next boot and used as the default selection.

---

## How Kernel Version Detection Works

ZhrBea reads offset `0x20E` of the vmlinuz binary. Since kernel 2.6, Linux embeds a null-terminated ASCII version string at this fixed offset in the bzImage header. ZhrBea reads 128 bytes, validates it looks like `N.N.*`, and appends it to the label automatically — no config needed.
