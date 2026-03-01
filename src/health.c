/* =============================================================================
 * health.c - Pre-boot system health diagnostics
 * Checks kernel, initramfs, EFI partition readability before showing menu.
 * No other bootloader does this!
 * =============================================================================
 */
#include <efi.h>
#include <efilib.h>
#include "../include/health.h"
#include "../include/config.h"

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

static BOOLEAN _check_file(EFI_FILE_HANDLE root, const CHAR16 *path, UINTN *size_out)
{
    EFI_FILE_HANDLE f = NULL;
    EFI_STATUS s = uefi_call_wrapper(root->Open, 5, root, &f,
                                     (CHAR16*)path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(s) || !f) return FALSE;

    if (size_out) {
        /* Get file size via GetInfo */
        EFI_FILE_INFO *info = NULL;
        UINTN info_sz = sizeof(EFI_FILE_INFO) + 256;
        info = AllocatePool(info_sz);
        if (info) {
            EFI_GUID fi_guid = EFI_FILE_INFO_ID;
            if (!EFI_ERROR(uefi_call_wrapper(f->GetInfo, 4, f, &fi_guid, &info_sz, info)))
                *size_out = (UINTN)info->FileSize;
            FreePool(info);
        }
    }

    uefi_call_wrapper(f->Close, 1, f);
    return TRUE;
}

static void _add(HealthReport *r, const char *label, HealthStatus st, const char *detail)
{
    if (r->count >= MAX_CHECKS) return;
    HealthCheck *c = &r->checks[r->count++];
    UINTN i;
    for (i = 0; label[i] && i < 63; i++) c->label[i] = label[i];
    c->label[i] = '\0';
    for (i = 0; detail[i] && i < 127; i++) c->detail[i] = detail[i];
    c->detail[i] = '\0';
    c->status = st;
    if (st > r->overall) r->overall = st;
}

/* Convert UINTN size to "X.X MB" string */
static void _fmt_size(UINTN bytes, char *buf, UINTN len)
{
    if (bytes == 0) { buf[0]='?'; buf[1]='\0'; return; }
    UINTN mb = bytes / (1024*1024);
    UINTN kb = (bytes % (1024*1024)) / 1024;
    /* Simple integer formatting */
    char tmp[32]; UINTN t = 0;
    if (mb > 0) {
        UINTN n = mb; char d[8]; UINTN dc=0;
        do { d[dc++]='0'+n%10; n/=10; } while(n&&dc<7);
        while(dc) tmp[t++]=d[--dc];
        tmp[t++]='.';
        n = kb/100; tmp[t++]='0'+(char)n;
        tmp[t++]=' '; tmp[t++]='M'; tmp[t++]='B';
    } else {
        UINTN n = kb; char d[8]; UINTN dc=0;
        do { d[dc++]='0'+n%10; n/=10; } while(n&&dc<7);
        while(dc) tmp[t++]=d[--dc];
        tmp[t++]=' '; tmp[t++]='K'; tmp[t++]='B';
    }
    tmp[t]='\0';
    UINTN i; for(i=0;i<t&&i<len-1;i++) buf[i]=tmp[i];
    buf[i]='\0';
}

void health_run(EFI_HANDLE ImageHandle, BootConfig *cfg, HealthReport *report)
{
    report->count   = 0;
    report->overall = HEALTH_OK;

    EFI_FILE_IO_INTERFACE *fio = _get_fs(ImageHandle);
    if (!fio) {
        _add(report, "EFI Filesystem", HEALTH_FAIL, "Cannot open volume");
        return;
    }

    EFI_FILE_HANDLE root = NULL;
    if (EFI_ERROR(uefi_call_wrapper(fio->OpenVolume, 2, fio, &root))) {
        _add(report, "EFI Filesystem", HEALTH_FAIL, "OpenVolume failed");
        return;
    }

    _add(report, "EFI Filesystem", HEALTH_OK, "Readable");

    /* Check each boot entry's kernel and initramfs */
    UINTN i;
    for (i = 0; i < cfg->EntryCount && report->count < MAX_CHECKS - 1; i++) {
        BootEntry *e = &cfg->Entries[i];
        if (e->Type != BOOT_TYPE_LINUX_STUB) continue;

        UINTN ksize = 0;
        BOOLEAN k_ok = _check_file(root, e->Path, &ksize);

        if (k_ok) {
            char detail[64];
            _fmt_size(ksize, detail, 64);
            _add(report, "Kernel", HEALTH_OK, detail);
        } else {
            _add(report, "Kernel", HEALTH_FAIL, "File not found!");
        }

        /* Check initramfs if specified */
        if (e->Initrd[0]) {
            UINTN isize = 0;
            BOOLEAN i_ok = _check_file(root, e->Initrd, &isize);
            if (i_ok) {
                char detail[64];
                _fmt_size(isize, detail, 64);
                _add(report, "Initramfs", HEALTH_OK, detail);
            } else {
                _add(report, "Initramfs", HEALTH_WARN, "Not found - may fail");
            }
        }
        break; /* Only check first linux entry */
    }

    /* Check config file exists */
    BOOLEAN cfg_ok = _check_file(root, L"\\EFI\\arch-limine\\bootloader.conf", NULL) ||
                     _check_file(root, L"\\EFI\\BOOT\\bootloader.conf", NULL);
    _add(report, "Config file", cfg_ok ? HEALTH_OK : HEALTH_WARN,
         cfg_ok ? "Found" : "Using built-in defaults");

    uefi_call_wrapper(root->Close, 1, root);
}