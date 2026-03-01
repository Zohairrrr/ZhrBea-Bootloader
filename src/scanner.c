/* =============================================================================
 * scanner.c  –  Auto-detect bootable OSes on the EFI partition
 *
 * Scans well-known EFI paths and detects:
 *   - Windows Boot Manager
 *   - Common Linux distro GRUB paths
 *   - Linux EFI stub kernels (vmlinuz*)
 *
 * For each vmlinuz found, reads the kernel version string from offset 0x20E
 * in the PE header (where Linux embeds it since kernel 2.6).
 * =============================================================================
 */
#include <efi.h>
#include <efilib.h>
#include "../include/scanner.h"
#include "../include/config.h"

/* ── File existence check ────────────────────────────────────────────────── */
static EFI_FILE_IO_INTERFACE *_get_fs(EFI_HANDLE ImageHandle)
{
    EFI_LOADED_IMAGE      *li  = NULL;
    EFI_FILE_IO_INTERFACE *fio = NULL;
    if (EFI_ERROR(uefi_call_wrapper(gBS->HandleProtocol, 3,
                  ImageHandle, &LoadedImageProtocol, (void**)&li))) return NULL;
    if (EFI_ERROR(uefi_call_wrapper(gBS->HandleProtocol, 3,
                  li->DeviceHandle, &FileSystemProtocol, (void**)&fio))) return NULL;
    return fio;
}

static BOOLEAN _file_exists(EFI_FILE_HANDLE root, const CHAR16 *path)
{
    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS s = uefi_call_wrapper(root->Open, 5, root, &f,
                                     (CHAR16*)path, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(s) && f) {
        uefi_call_wrapper(f->Close, 1, f);
        return TRUE;
    }
    return FALSE;
}

/* ── Kernel version from PE header ──────────────────────────────────────── */
BOOLEAN scanner_kernel_version(EFI_HANDLE ImageHandle,
                                const CHAR16 *kpath,
                                char *out, UINTN out_len)
{
    EFI_FILE_IO_INTERFACE *fio = _get_fs(ImageHandle);
    if (!fio) return FALSE;

    EFI_FILE_HANDLE root = NULL, f = NULL;
    if (EFI_ERROR(uefi_call_wrapper(fio->OpenVolume, 2, fio, &root))) return FALSE;
    if (EFI_ERROR(uefi_call_wrapper(root->Open, 5, root, &f,
                  (CHAR16*)kpath, EFI_FILE_MODE_READ, 0))) {
        uefi_call_wrapper(root->Close, 1, root);
        return FALSE;
    }

    /*
     * Linux kernel version string is at offset 0x20E from the start of the
     * bzImage / EFI stub.  It's a null-terminated ASCII string like
     * "6.6.8-arch1-1".
     */
    UINT8  buf[128];
    UINTN  sz = sizeof(buf);
    UINT64 off = 0x20E;

    uefi_call_wrapper(f->SetPosition, 2, f, off);
    EFI_STATUS s = uefi_call_wrapper(f->Read, 3, f, &sz, buf);
    uefi_call_wrapper(f->Close, 1, f);
    uefi_call_wrapper(root->Close, 1, root);

    if (EFI_ERROR(s)) return FALSE;

    /* Copy printable ASCII chars */
    UINTN i, o;
    for (i = 0, o = 0; i < sz && o < out_len - 1; i++) {
        if (buf[i] == 0) break;
        if (buf[i] >= 0x20 && buf[i] < 0x7F)
            out[o++] = (char)buf[i];
        else if (o > 4) break;  /* stop at first weird byte after some chars */
    }
    out[o] = '\0';

    /* Sanity: must look like "N.N" at minimum */
    return (o >= 3 && out[1] == '.');
}

/* ── Known EFI paths to probe ────────────────────────────────────────────── */
typedef struct {
    const CHAR16 *path;
    const CHAR16 *label;
    BootType       type;
    const CHAR16 *initrd;   /* NULL for EFI type */
    const CHAR16 *args;
} KnownEntry;

static const KnownEntry known[] = {
    /* Windows */
    { L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
      L"Windows Boot Manager", BOOT_TYPE_EFI, NULL, L"" },

    /* Common Linux GRUB installs */
    { L"\\EFI\\ubuntu\\grubx64.efi",
      L"Ubuntu Linux", BOOT_TYPE_EFI, NULL, L"" },
    { L"\\EFI\\fedora\\grubx64.efi",
      L"Fedora Linux", BOOT_TYPE_EFI, NULL, L"" },
    { L"\\EFI\\opensuse\\grubx64.efi",
      L"openSUSE", BOOT_TYPE_EFI, NULL, L"" },
    { L"\\EFI\\debian\\grubx64.efi",
      L"Debian Linux", BOOT_TYPE_EFI, NULL, L"" },
    { L"\\EFI\\manjaro\\grubx64.efi",
      L"Manjaro Linux", BOOT_TYPE_EFI, NULL, L"" },
    { L"\\EFI\\endeavouros\\grubx64.efi",
      L"EndeavourOS", BOOT_TYPE_EFI, NULL, L"" },

    /* Arch-based direct stubs */
    { L"\\vmlinuz-linux",
      L"Arch Linux", BOOT_TYPE_LINUX_STUB,
      L"\\initramfs-linux.img",
      L"root=/dev/sda2 rw quiet loglevel=3" },

    { L"\\vmlinuz-linux-lts",
      L"Arch Linux LTS", BOOT_TYPE_LINUX_STUB,
      L"\\initramfs-linux-lts.img",
      L"root=/dev/sda2 rw quiet loglevel=3" },

    { L"\\vmlinuz-linux-zen",
      L"Arch Linux Zen", BOOT_TYPE_LINUX_STUB,
      L"\\initramfs-linux-zen.img",
      L"root=/dev/sda2 rw quiet loglevel=3" },
};

#define KNOWN_COUNT (sizeof(known)/sizeof(known[0]))

/* ── Main scanner ────────────────────────────────────────────────────────── */
void scanner_run(EFI_HANDLE ImageHandle, BootConfig *cfg)
{
    EFI_FILE_IO_INTERFACE *fio = _get_fs(ImageHandle);
    if (!fio) return;

    EFI_FILE_HANDLE root = NULL;
    if (EFI_ERROR(uefi_call_wrapper(fio->OpenVolume, 2, fio, &root))) return;

    UINTN k;
    for (k = 0; k < KNOWN_COUNT; k++) {
        if (cfg->EntryCount >= MAX_ENTRIES) break;
        if (!_file_exists(root, known[k].path)) continue;

        /* Check if already in config (avoid duplicates) */
        BOOLEAN dup = FALSE;
        UINTN j;
        for (j = 0; j < cfg->EntryCount; j++) {
            if (StrCmp(cfg->Entries[j].Path, known[k].path) == 0) {
                dup = TRUE; break;
            }
        }
        if (dup) continue;

        BootEntry *e = &cfg->Entries[cfg->EntryCount++];
        StrCpy(e->Path,   (CHAR16*)known[k].path);
        StrCpy(e->Args,   known[k].args ? (CHAR16*)known[k].args : L"");
        StrCpy(e->Initrd, known[k].initrd ? (CHAR16*)known[k].initrd : L"");
        e->Type      = known[k].type;
        e->IsDefault = FALSE;

        /* Build label: "Arch Linux (6.6.8-arch1-1)" */
        StrCpy(e->Label, (CHAR16*)known[k].label);

        if (known[k].type == BOOT_TYPE_LINUX_STUB) {
            char ver[64] = {0};
            if (scanner_kernel_version(ImageHandle, known[k].path, ver, 64)
                && ver[0]) {
                /* Append " (version)" to label */
                UINTN llen = StrLen(e->Label);
                e->Label[llen++] = L' ';
                e->Label[llen++] = L'(';
                UINTN vi = 0;
                while (ver[vi] && llen < MAX_LABEL_LEN - 2)
                    e->Label[llen++] = (CHAR16)(UINT8)ver[vi++];
                e->Label[llen++] = L')';
                e->Label[llen]   = L'\0';
            }
        }
    }

    uefi_call_wrapper(root->Close, 1, root);
}