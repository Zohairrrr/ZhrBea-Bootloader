#ifndef BOOT_H
#define BOOT_H

#include <efi.h>
#include <efilib.h>
#include "config.h"

/* ── Result codes ────────────────────────────────────────────────────────── */
typedef enum {
    BOOT_OK = 0,
    BOOT_ERR_PATH_NOT_FOUND,
    BOOT_ERR_LOAD_FAILED,
    BOOT_ERR_START_FAILED,
    BOOT_ERR_INITRD_NOT_FOUND,
} BootResult;

/* ── Public API ──────────────────────────────────────────────────────────── */

/*
 * boot_entry()
 *   Boot the OS described by `entry`.
 *   Returns BOOT_OK (rare – normally the OS takes over and we never return),
 *   or an error code if the boot failed.
 */
BootResult boot_entry(EFI_HANDLE      ImageHandle,
                      EFI_SYSTEM_TABLE *ST,
                      const BootEntry *entry);

/*
 * boot_result_string()
 *   Human-readable description of a BootResult error code.
 */
const CHAR16 *boot_result_string(BootResult r);

#endif /* BOOT_H */