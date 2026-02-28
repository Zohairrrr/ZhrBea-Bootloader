#ifndef CONFIG_H
#define CONFIG_H

#include <efi.h>
#include <efilib.h>

/* ── Limits ──────────────────────────────────────────────────────────────── */
#define MAX_ENTRIES     16
#define MAX_LABEL_LEN   64
#define MAX_PATH_LEN   256
#define MAX_ARGS_LEN   512
#define MAX_TYPE_LEN    32

/* ── Boot entry types ────────────────────────────────────────────────────── */
typedef enum {
    BOOT_TYPE_EFI,          /* Generic EFI application (Windows, GRUB, rEFInd …) */
    BOOT_TYPE_LINUX_STUB,   /* Direct Linux EFI stub kernel + initrd + cmdline  */
    BOOT_TYPE_CHAIN,        /* Chainload another EFI bootloader                 */
} BootType;

/* ── One OS / boot entry ─────────────────────────────────────────────────── */
typedef struct {
    CHAR16  Label[MAX_LABEL_LEN];    /* Display name                         */
    CHAR16  Path[MAX_PATH_LEN];      /* Path to the EFI binary or kernel     */
    CHAR16  Initrd[MAX_PATH_LEN];    /* (Linux stub only) initrd path        */
    CHAR16  Args[MAX_ARGS_LEN];      /* Kernel cmdline or EFI load options   */
    BootType Type;
    BOOLEAN IsDefault;               /* TRUE → selected on timeout           */
} BootEntry;

/* ── Global config ───────────────────────────────────────────────────────── */
typedef struct {
    BootEntry Entries[MAX_ENTRIES];
    UINTN     EntryCount;
    UINTN     DefaultIndex;          /* Index of the default entry           */
    UINTN     TimeoutSeconds;        /* 0 = wait forever                     */
    BOOLEAN   Verbose;               /* Extra debug output during boot       */
} BootConfig;

/* ── Public API ──────────────────────────────────────────────────────────── */

/*
 * config_load()
 *   Read \EFI\BOOT\bootloader.conf from the same device that loaded us.
 *   Populates `cfg` and returns EFI_SUCCESS, or an error code.
 *   If the file is missing, a built-in default config is used.
 */
EFI_STATUS config_load(EFI_HANDLE ImageHandle,
                        EFI_SYSTEM_TABLE *ST,
                        BootConfig *cfg);

/*
 * config_load_defaults()
 *   Fill `cfg` with compiled-in defaults so the bootloader is always usable
 *   even without a config file on disk.
 */
void config_load_defaults(BootConfig *cfg);

#endif /* CONFIG_H */