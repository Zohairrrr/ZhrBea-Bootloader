#ifndef SCANNER_H
#define SCANNER_H

#include <efi.h>
#include <efilib.h>
#include "config.h"

/* Scan the EFI partition for bootable entries and kernel versions.
 * Appends found entries to cfg (up to MAX_ENTRIES).
 * Also fills in kernel version strings where detectable. */
void scanner_run(EFI_HANDLE ImageHandle, BootConfig *cfg);

/* Try to read kernel version from a vmlinuz file.
 * Writes into out_buf (max out_len chars). Returns FALSE if unreadable. */
BOOLEAN scanner_kernel_version(EFI_HANDLE ImageHandle,
                                const CHAR16 *kernel_path,
                                char *out_buf, UINTN out_len);
#endif