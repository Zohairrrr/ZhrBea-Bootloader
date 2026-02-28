# =============================================================================
# Makefile – UEFI Bootloader
# =============================================================================

# ── Toolchain ────────────────────────────────────────────────────────────────
ARCH      = x86_64
CC        = gcc
LD        = ld
OBJCOPY   = objcopy

# ── gnu-efi paths (adjust if your distro puts them elsewhere) ────────────────
#   Arch:   pacman -S gnu-efi
#   Fedora: dnf install gnu-efi gnu-efi-devel
#   Debian: apt install gnu-efi
EFIINC    = /usr/include/efi
EFILIB    = /usr/lib

EFI_CRT   = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS   = $(EFILIB)/elf_$(ARCH)_efi.lds

# ── Source files ─────────────────────────────────────────────────────────────
SRCS = src/main.c \
       src/ui.c   \
       src/config.c \
       src/boot.c

OBJS = $(SRCS:.c=.o)

# ── Output names ─────────────────────────────────────────────────────────────
TARGET_SO  = bootloader.so
TARGET_EFI = bootloader.efi

# ── Compiler flags ───────────────────────────────────────────────────────────
CFLAGS = \
    -I$(EFIINC)              \
    -I$(EFIINC)/$(ARCH)      \
    -I$(EFIINC)/protocol     \
    -Iinclude                \
    -fno-stack-protector     \
    -fpic                    \
    -fshort-wchar            \
    -mno-red-zone            \
    -Wall                    \
    -Wextra                  \
    -Wno-unused-parameter    \
    -DGNU_EFI_USE_MS_ABI     \
    -O2

# ── Linker flags ─────────────────────────────────────────────────────────────
LDFLAGS = \
    -nostdlib               \
    -znocombreloc           \
    -T $(EFI_LDS)           \
    -shared                 \
    -Bsymbolic              \
    -L $(EFILIB)            \
    $(EFI_CRT)

# ── QEMU / disk image settings ───────────────────────────────────────────────
IMAGE_DIR   = image
EFI_BOOT    = $(IMAGE_DIR)/EFI/BOOT
OVMF        = /usr/share/edk2/x64/OVMF.4m.fd

# Also try the OVMF path used on Debian/Ubuntu and Fedora
ifeq ($(wildcard $(OVMF)),)
    OVMF = /usr/share/ovmf/OVMF.fd
endif
ifeq ($(wildcard $(OVMF)),)
    OVMF = /usr/share/edk2-ovmf/OVMF.fd
endif

QEMU_FLAGS = \
    -bios $(OVMF)                              \
    -drive format=raw,file=fat:rw:$(IMAGE_DIR) \
    -m 512M                                    \
    -serial stdio                              \
    -no-reboot

# =============================================================================
# Targets
# =============================================================================

.PHONY: all clean run install-deps check-deps

all: check-deps $(TARGET_EFI)

# ── Compile C → object files ─────────────────────────────────────────────────
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Link objects → shared library ────────────────────────────────────────────
$(TARGET_SO): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@ -lefi -lgnuefi

# ── Convert shared library → UEFI PE32+ binary ───────────────────────────────
$(TARGET_EFI): $(TARGET_SO)
	$(OBJCOPY) \
	    -j .text    \
	    -j .sdata   \
	    -j .data    \
	    -j .rodata  \
	    -j .dynamic \
	    -j .dynsym  \
	    -j .rel     \
	    -j .rela    \
	    -j .rel.*   \
	    -j .rela.*  \
	    -j .reloc   \
	    -O efi-app-$(ARCH) \
	    $< $@
	@echo ""
	@echo "  ✓  Built $(TARGET_EFI)  ($(shell du -sh $(TARGET_EFI) | cut -f1))"
	@echo ""

# ── Set up FAT disk image tree ────────────────────────────────────────────────
image: $(TARGET_EFI)
	mkdir -p $(EFI_BOOT)
	cp $(TARGET_EFI) $(EFI_BOOT)/BOOTX64.EFI
	@if [ ! -f $(EFI_BOOT)/bootloader.conf ]; then \
	    cp bootloader.conf.example $(EFI_BOOT)/bootloader.conf; \
	    echo "  ✓  Created $(EFI_BOOT)/bootloader.conf — edit root= to match your partition!"; \
	fi
	@echo "  ✓  Image ready at $(IMAGE_DIR)/"

# ── Run in QEMU ───────────────────────────────────────────────────────────────
run: image
	@echo "  → Launching QEMU..."
	@echo "     OVMF: $(OVMF)"
	qemu-system-x86_64 $(QEMU_FLAGS)

# ── Run with QEMU in debug mode (serial output to terminal) ──────────────────
debug: image
	qemu-system-x86_64 $(QEMU_FLAGS) \
	    -d int,cpu_reset -D /tmp/qemu-debug.log

# ── Clean build artefacts ─────────────────────────────────────────────────────
clean:
	rm -f src/*.o $(TARGET_SO) $(TARGET_EFI)

# ── Clean everything including the disk image ─────────────────────────────────
distclean: clean
	rm -rf $(IMAGE_DIR)

# ── Install build dependencies (Arch Linux) ───────────────────────────────────
install-deps-arch:
	sudo pacman -S --needed gnu-efi qemu ovmf

# ── Install build dependencies (Debian / Ubuntu) ─────────────────────────────
install-deps-debian:
	sudo apt install -y gnu-efi ovmf qemu-system-x86

# ── Install build dependencies (Fedora) ───────────────────────────────────────
install-deps-fedora:
	sudo dnf install -y gnu-efi gnu-efi-devel edk2-ovmf qemu-system-x86

# ── Sanity check ─────────────────────────────────────────────────────────────
check-deps:
	@test -f $(EFI_CRT) || (echo "ERROR: gnu-efi not found at $(EFI_CRT)"; exit 1)
	@test -f $(EFI_LDS) || (echo "ERROR: EFI linker script not found at $(EFI_LDS)"; exit 1)
	@test -f $(OVMF)    || (echo "ERROR: OVMF firmware not found at $(OVMF)"; exit 1)