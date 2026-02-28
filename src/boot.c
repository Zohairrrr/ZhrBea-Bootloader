/* =============================================================================
 * boot.c  –  OS boot execution
 *
 * Handles three boot types:
 *   BOOT_TYPE_EFI        – generic EFI application (Windows, GRUB, rEFInd…)
 *   BOOT_TYPE_LINUX_STUB – direct Linux EFI stub (kernel + initrd + cmdline)
 *   BOOT_TYPE_CHAIN      – chainload another EFI binary
 *
 * For the Linux EFI stub we build a proper initrd=\path string and inject it
 * into LoadOptions so the kernel finds its initramfs.
 * =============================================================================
 */

#include <efi.h>
#include <efilib.h>
#include "../include/boot.h"
#include "../include/config.h"

/* ── Internal helpers ────────────────────────────────────────────────────── */

/*
 * _load_and_start()
 *   Core helper: resolve `path` to a DevicePath relative to our own device,
 *   load the EFI image, optionally inject `load_opts`, then start it.
 */
static BootResult _load_and_start(EFI_HANDLE        ImageHandle,
                                   EFI_SYSTEM_TABLE  *ST,
                                   const CHAR16      *path,
                                   const CHAR16      *load_opts)
{
    EFI_LOADED_IMAGE    *my_image  = NULL;
    EFI_DEVICE_PATH     *dev_path  = NULL;
    EFI_HANDLE           new_hdl   = NULL;
    EFI_STATUS           s;

    /* 1. Get our own loaded-image so we can borrow its DeviceHandle */
    s = uefi_call_wrapper(gBS->HandleProtocol, 3,
                          ImageHandle,
                          &LoadedImageProtocol,
                          (void **)&my_image);
    if (EFI_ERROR(s)) return BOOT_ERR_LOAD_FAILED;

    /* 2. Build an absolute device path for `path` on the same partition */
    dev_path = FileDevicePath(my_image->DeviceHandle, (CHAR16 *)path);
    if (!dev_path) return BOOT_ERR_PATH_NOT_FOUND;

    /* 3. Load the image */
    s = uefi_call_wrapper(gBS->LoadImage, 6,
                          (UINTN)FALSE,
                          ImageHandle,
                          dev_path,
                          NULL, 0,
                          &new_hdl);
    FreePool(dev_path);
    if (EFI_ERROR(s)) return BOOT_ERR_LOAD_FAILED;

    /* 4. Inject load options / kernel command line if provided */
    if (load_opts && StrLen(load_opts) > 0) {
        EFI_LOADED_IMAGE *target = NULL;
        s = uefi_call_wrapper(gBS->HandleProtocol, 3,
                              new_hdl,
                              &LoadedImageProtocol,
                              (void **)&target);
        if (!EFI_ERROR(s)) {
            UINTN opts_size = (StrLen(load_opts) + 1) * sizeof(CHAR16);
            /* Allocate a persistent copy – the kernel will read it after we
             * call StartImage, so it must stay valid. */
            CHAR16 *opts_copy = AllocatePool(opts_size);
            if (opts_copy) {
                CopyMem(opts_copy, (void *)load_opts, opts_size);
                target->LoadOptions     = opts_copy;
                target->LoadOptionsSize = (UINT32)opts_size;
            }
        }
    }

    /* 5. Start the image – if this returns we had an error */
    s = uefi_call_wrapper(gBS->StartImage, 3, new_hdl, NULL, NULL);
    if (EFI_ERROR(s)) return BOOT_ERR_START_FAILED;

    return BOOT_OK;   /* not normally reached */
}

/*
 * _build_linux_opts()
 *   Combine the user-supplied kernel args with the initrd= parameter.
 *   Result written into `out_buf` (caller provides MAX_ARGS_LEN-sized buffer).
 *
 *   The Linux EFI stub reads LoadOptions as a UTF-16 string.
 *   The initrd= path must use the UEFI file-path syntax with backslashes.
 */
static void _build_linux_opts(const CHAR16 *args,
                               const CHAR16 *initrd,
                               CHAR16       *out_buf,
                               UINTN         buf_chars)
{
    out_buf[0] = L'\0';

    /* Start with user args */
    if (args && StrLen(args) > 0) {
        StrCpy(out_buf, args);
    }

    /* Append initrd= if given */
    if (initrd && StrLen(initrd) > 0) {
        if (StrLen(out_buf) > 0 && StrLen(out_buf) < buf_chars - 1)
            StrCat(out_buf, L" ");
        StrCat(out_buf, L"initrd=");
        StrCat(out_buf, initrd);
    }
}

/* ── Public functions ────────────────────────────────────────────────────── */

BootResult boot_entry(EFI_HANDLE      ImageHandle,
                      EFI_SYSTEM_TABLE *ST,
                      const BootEntry *entry)
{
    switch (entry->Type) {

    case BOOT_TYPE_EFI:
    case BOOT_TYPE_CHAIN:
        /* Just load and run the EFI application, optionally passing args */
        return _load_and_start(ImageHandle, ST, entry->Path,
                               (StrLen(entry->Args) > 0) ? entry->Args : NULL);

    case BOOT_TYPE_LINUX_STUB: {
        /* Build "user_args initrd=\path" and inject into LoadOptions */
        CHAR16 combined[MAX_ARGS_LEN];
        _build_linux_opts(entry->Args, entry->Initrd,
                          combined, MAX_ARGS_LEN);
        return _load_and_start(ImageHandle, ST, entry->Path, combined);
    }

    default:
        return BOOT_ERR_LOAD_FAILED;
    }
}

const CHAR16 *boot_result_string(BootResult r)
{
    switch (r) {
    case BOOT_OK:                 return L"Success";
    case BOOT_ERR_PATH_NOT_FOUND: return L"EFI file not found on this partition";
    case BOOT_ERR_LOAD_FAILED:    return L"Failed to load EFI image";
    case BOOT_ERR_START_FAILED:   return L"Image loaded but failed to start";
    case BOOT_ERR_INITRD_NOT_FOUND: return L"Initrd not found";
    default:                      return L"Unknown error";
    }
}