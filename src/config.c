/* =============================================================================
 * config.c  –  Configuration file parser
 *
 * Uses ONLY gnu-efi native type names (not EDK2 names).
 *   EFI_FILE_IO_INTERFACE  (not EFI_SIMPLE_FILE_SYSTEM_PROTOCOL)
 *   FileSystemProtocol     (not gEfiSimpleFileSystemProtocolGuid)
 * =============================================================================
 */

#include <efi.h>
#include <efilib.h>
#include "../include/config.h"

/* ── Compile-in fallback entries ─────────────────────────────────────────── */
void config_load_defaults(BootConfig *cfg)
{
    cfg->EntryCount     = 0;
    cfg->DefaultIndex   = 0;
    cfg->TimeoutSeconds = 5;
    cfg->Verbose        = FALSE;

#define ADD(lbl, pth, ini, arg, typ, def)                        \
    do {                                                          \
        if (cfg->EntryCount >= MAX_ENTRIES) break;               \
        BootEntry *e = &cfg->Entries[cfg->EntryCount++];         \
        StrCpy(e->Label,  (CHAR16 *)(lbl));                      \
        StrCpy(e->Path,   (CHAR16 *)(pth));                      \
        StrCpy(e->Initrd, (CHAR16 *)(ini));                      \
        StrCpy(e->Args,   (CHAR16 *)(arg));                      \
        e->Type      = (typ);                                     \
        e->IsDefault = (def);                                     \
    } while (0)

    ADD(L"Arch Linux",
        L"\\vmlinuz-linux",
        L"\\initramfs-linux.img",
        L"root=/dev/sda2 rw quiet loglevel=3",
        BOOT_TYPE_LINUX_STUB, TRUE);

    ADD(L"Arch Linux (verbose)",
        L"\\vmlinuz-linux",
        L"\\initramfs-linux.img",
        L"root=/dev/sda2 rw loglevel=7 earlyprintk",
        BOOT_TYPE_LINUX_STUB, FALSE);

    ADD(L"Arch Linux (fallback initramfs)",
        L"\\vmlinuz-linux",
        L"\\initramfs-linux-fallback.img",
        L"root=/dev/sda2 rw",
        BOOT_TYPE_LINUX_STUB, FALSE);

    ADD(L"Windows 11 (Boot Manager)",
        L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
        L"", L"",
        BOOT_TYPE_EFI, FALSE);

    ADD(L"UEFI Shell",
        L"\\EFI\\BOOT\\Shell.efi",
        L"", L"",
        BOOT_TYPE_EFI, FALSE);

#undef ADD

    cfg->DefaultIndex = 0;
}

/* ── String helpers ──────────────────────────────────────────────────────── */

static BOOLEAN _is_whitespace(CHAR16 c)
{
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n';
}

static CHAR16 *_trim(CHAR16 *s)
{
    while (*s && _is_whitespace(*s)) s++;
    UINTN len = StrLen(s);
    while (len > 0 && _is_whitespace(s[len - 1])) s[--len] = L'\0';
    return s;
}

static BOOLEAN _key_eq(const CHAR16 *a, const CHAR16 *b)
{
    while (*a && *b) {
        CHAR16 ca = (*a >= L'A' && *a <= L'Z') ? (*a + 32) : *a;
        CHAR16 cb = (*b >= L'A' && *b <= L'Z') ? (*b + 32) : *b;
        if (ca != cb) return FALSE;
        a++; b++;
    }
    return *a == *b;
}

static UINTN _parse_uint(const CHAR16 *s)
{
    UINTN v = 0;
    while (*s >= L'0' && *s <= L'9')
        v = v * 10 + (*s++ - L'0');
    return v;
}

/* ── File reading using gnu-efi native types ─────────────────────────────── */

#define CONFIG_PATH  L"\\EFI\\BOOT\\bootloader.conf"
#define READ_BUF_SZ  (16 * 1024)

static EFI_STATUS _read_file(EFI_HANDLE ImageHandle,
                              UINT8 **out_buf, UINTN *out_size)
{
    EFI_LOADED_IMAGE      *li   = NULL;
    EFI_FILE_IO_INTERFACE *fio  = NULL;   /* gnu-efi name for SimpleFileSystem */
    EFI_FILE_HANDLE        root = NULL;
    EFI_FILE_HANDLE        file = NULL;
    UINT8                 *buf  = NULL;
    EFI_STATUS             s;

    s = uefi_call_wrapper(gBS->HandleProtocol, 3,
                          ImageHandle,
                          &LoadedImageProtocol,
                          (void **)&li);
    if (EFI_ERROR(s)) goto cleanup;

    /* FileSystemProtocol is the gnu-efi GUID for SimpleFileSystem */
    s = uefi_call_wrapper(gBS->HandleProtocol, 3,
                          li->DeviceHandle,
                          &FileSystemProtocol,
                          (void **)&fio);
    if (EFI_ERROR(s)) goto cleanup;

    s = uefi_call_wrapper(fio->OpenVolume, 2, fio, &root);
    if (EFI_ERROR(s)) goto cleanup;

    s = uefi_call_wrapper(root->Open, 5, root, &file,
                          CONFIG_PATH, EFI_FILE_MODE_READ, (UINT64)0);
    if (EFI_ERROR(s)) goto cleanup;

    buf = AllocatePool(READ_BUF_SZ);
    if (!buf) { s = EFI_OUT_OF_RESOURCES; goto cleanup; }

    *out_size = READ_BUF_SZ - 2;
    s = uefi_call_wrapper(file->Read, 3, file, out_size, buf);
    if (EFI_ERROR(s)) { FreePool(buf); buf = NULL; goto cleanup; }

    buf[*out_size]     = 0;
    buf[*out_size + 1] = 0;
    *out_buf = buf;

cleanup:
    if (file) uefi_call_wrapper(file->Close, 1, file);
    if (root) uefi_call_wrapper(root->Close, 1, root);
    return s;
}

/* ── Convert ASCII/UTF-8 bytes to CHAR16 ─────────────────────────────────── */
static void _utf8_to_wide(const UINT8 *src, CHAR16 *dst, UINTN max_dst)
{
    UINTN i = 0;
    while (*src && i < max_dst - 1)
        dst[i++] = (CHAR16)(*src++);
    dst[i] = L'\0';
}

/* ── Main parser ─────────────────────────────────────────────────────────── */
EFI_STATUS config_load(EFI_HANDLE ImageHandle,
                        EFI_SYSTEM_TABLE *ST,
                        BootConfig *cfg)
{
    (void)ST;

    /* Start with safe defaults */
    config_load_defaults(cfg);

    UINT8 *raw  = NULL;
    UINTN  size = 0;
    EFI_STATUS s = _read_file(ImageHandle, &raw, &size);
    if (EFI_ERROR(s)) {
        /* Config file missing – built-in defaults are fine */
        return EFI_SUCCESS;
    }

    /* Rebuild entry list from file */
    cfg->EntryCount = 0;

    BootEntry *cur = NULL;
    UINT8 *p = raw;

    while (*p) {
        UINT8 *line_start = p;
        while (*p && *p != '\n') p++;
        if (*p == '\n') *p++ = '\0';

        CHAR16 wide_line[512];
        _utf8_to_wide(line_start, wide_line, 512);
        CHAR16 *line = _trim(wide_line);

        if (!*line || *line == L'#' || *line == L';') continue;

        /* Section header [entry] */
        if (*line == L'[') {
            CHAR16 *end = line + 1;
            while (*end && *end != L']') end++;
            *end = L'\0';
            CHAR16 *section = _trim(line + 1);
            if (_key_eq(section, L"entry") && cfg->EntryCount < MAX_ENTRIES) {
                cur = &cfg->Entries[cfg->EntryCount++];
                cur->Label[0]  = L'\0';
                cur->Path[0]   = L'\0';
                cur->Initrd[0] = L'\0';
                cur->Args[0]   = L'\0';
                cur->Type      = BOOT_TYPE_EFI;
                cur->IsDefault = FALSE;
            } else {
                cur = NULL;
            }
            continue;
        }

        /* key = value */
        CHAR16 *eq = line;
        while (*eq && *eq != L'=') eq++;
        if (!*eq) continue;
        *eq = L'\0';
        CHAR16 *key = _trim(line);
        CHAR16 *val = _trim(eq + 1);

        if (!cur) {
            if (_key_eq(key, L"timeout"))
                cfg->TimeoutSeconds = _parse_uint(val);
            else if (_key_eq(key, L"default"))
                cfg->DefaultIndex = _parse_uint(val);
            else if (_key_eq(key, L"verbose"))
                cfg->Verbose = _key_eq(val, L"true") || _key_eq(val, L"1");
        } else {
            if      (_key_eq(key, L"label"))  StrCpy(cur->Label,  val);
            else if (_key_eq(key, L"path"))   StrCpy(cur->Path,   val);
            else if (_key_eq(key, L"initrd")) StrCpy(cur->Initrd, val);
            else if (_key_eq(key, L"args") || _key_eq(key, L"options"))
                StrCpy(cur->Args, val);
            else if (_key_eq(key, L"type")) {
                if      (_key_eq(val, L"linux")) cur->Type = BOOT_TYPE_LINUX_STUB;
                else if (_key_eq(val, L"chain")) cur->Type = BOOT_TYPE_CHAIN;
                else                             cur->Type = BOOT_TYPE_EFI;
            } else if (_key_eq(key, L"default")) {
                cur->IsDefault = _key_eq(val, L"true") || _key_eq(val, L"1");
                if (cur->IsDefault)
                    cfg->DefaultIndex = (UINTN)(cur - cfg->Entries);
            }
        }
    }

    FreePool(raw);

    if (cfg->EntryCount == 0) config_load_defaults(cfg);
    if (cfg->DefaultIndex >= cfg->EntryCount) cfg->DefaultIndex = 0;

    return EFI_SUCCESS;
}