/* =============================================================================
 * history.c  –  Boot history via UEFI NVRAM variable
 *
 * We store a single UINT32 (the last-booted entry index) under the vendor GUID.
 * This persists across reboots in NVRAM — exactly like how UEFI stores boot order.
 * =============================================================================
 */
#include <efi.h>
#include <efilib.h>
#include "../include/history.h"

/* Our private vendor GUID for the NVRAM variable */
/* {A1B2C3D4-E5F6-7890-ABCD-EF1234567890} */
static EFI_GUID HistoryGuid = {
    0xA1B2C3D4, 0xE5F6, 0x7890,
    {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x90}
};

static CHAR16 *VAR_NAME = L"BootloaderLastEntry";

void history_save(UINTN index)
{
    UINT32 val = (UINT32)index;
    uefi_call_wrapper(gRT->SetVariable, 5,
                      VAR_NAME,
                      &HistoryGuid,
                      EFI_VARIABLE_NON_VOLATILE |
                      EFI_VARIABLE_BOOTSERVICE_ACCESS |
                      EFI_VARIABLE_RUNTIME_ACCESS,
                      sizeof(UINT32),
                      &val);
}

BOOLEAN history_load(UINTN *index_out)
{
    UINT32 val = 0;
    UINTN  sz  = sizeof(UINT32);
    EFI_STATUS s = uefi_call_wrapper(gRT->GetVariable, 5,
                                     VAR_NAME,
                                     &HistoryGuid,
                                     NULL,
                                     &sz,
                                     &val);
    if (EFI_ERROR(s)) return FALSE;
    *index_out = (UINTN)val;
    return TRUE;
}