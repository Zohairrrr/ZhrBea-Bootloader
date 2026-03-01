#ifndef HISTORY_H
#define HISTORY_H

#include <efi.h>
#include <efilib.h>

/* Saves last-booted entry index to UEFI NVRAM variable.
 * On next boot, the saved index becomes the default selection. */

void     history_save(UINTN index);
BOOLEAN  history_load(UINTN *index_out);  /* returns FALSE if no history saved */

#endif