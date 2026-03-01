/* =============================================================================
 * main.c  -  UEFI Bootloader entry point
 * Features: GOP graphics, splash animation, boot history, OS auto-detection
 * =============================================================================
 */
#include <efi.h>
#include <efilib.h>
#include "../include/ui.h"
#include "../include/config.h"
#include "../include/boot.h"
#include "../include/history.h"
#include "../include/scanner.h"
#include "../include/health.h"

static void draw_all(const BootConfig *cfg, UINTN sel)
{
    UINTN i;
    for (i = 0; i < cfg->EntryCount; i++)
        ui_draw_menu_row(i, cfg->Entries[i].Label, (BOOLEAN)(i == sel));
}

static void do_boot(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST,
                    const BootConfig *cfg, UINTN sel)
{
    /* Save to history before attempting boot */
    history_save(sel);

    ui_set_status(COL_HIGHLIGHT, L"Booting... please wait");

    BootResult r = boot_entry(ImageHandle, ST, &cfg->Entries[sel]);

    /* Only here on failure */
    CHAR16 msg[200];
    UINTN i = 0;
    const CHAR16 *pre = L"Boot failed: ";
    const CHAR16 *rsn = boot_result_string(r);
    while (*pre && i < 198) msg[i++] = *pre++;
    while (*rsn && i < 198) msg[i++] = *rsn++;
    msg[i] = L'\0';
    ui_set_status(COL_ERROR, msg);

    EFI_INPUT_KEY k;
    while (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &k) != EFI_SUCCESS)
        uefi_call_wrapper(gBS->Stall, 1, (UINTN)10000);
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST)
{
    InitializeLib(ImageHandle, ST);

    /* ui_init runs splash animation */
    ui_init(ST);

    /* Load config */
    BootConfig cfg;
    config_load(ImageHandle, ST, &cfg);

    /* Auto-scan for additional OSes not in config */
    scanner_run(ImageHandle, &cfg);

    if (cfg.EntryCount == 0) {
        ui_set_status(COL_ERROR, L"No boot entries found.");
        for (;;) uefi_call_wrapper(gBS->Stall, 1, (UINTN)500000);
    }

    /* Apply boot history: override default if we have a saved last-boot */
    UINTN hist_idx = 0;
    if (history_load(&hist_idx) && hist_idx < cfg.EntryCount)
        cfg.DefaultIndex = hist_idx;

    /* Run health checks */
    HealthReport health;
    health_run(ImageHandle, &cfg, &health);

    /* Draw menu */
    UINTN selected = cfg.DefaultIndex;
    ui_draw_chrome();
    draw_all(&cfg, selected);
    ui_draw_health(&health);

    /* Timer */
    UINTN    countdown    = cfg.TimeoutSeconds;
    BOOLEAN  timer_on     = (countdown > 0);
    UINTN    stall_acc    = 0;
    if (timer_on) ui_update_timer(countdown);

    /* Event loop */
    for (;;) {
        if (timer_on) {
            uefi_call_wrapper(gBS->Stall, 1, (UINTN)10000);
            stall_acc += 10000;
            if (stall_acc >= 1000000) {
                stall_acc = 0;
                if (countdown) countdown--;
                ui_update_timer(countdown);
                if (countdown == 0) {
                    timer_on = FALSE;
                    do_boot(ImageHandle, ST, &cfg, selected);
                    ui_full_redraw(); draw_all(&cfg, selected);
                    countdown = cfg.TimeoutSeconds;
                    timer_on  = (countdown > 0);
                    stall_acc = 0;
                    if (timer_on) ui_update_timer(countdown);
                    continue;
                }
            }
        } else {
            uefi_call_wrapper(gBS->Stall, 1, (UINTN)5000);
        }

        EFI_INPUT_KEY key;
        if (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &key) != EFI_SUCCESS)
            continue;

        if (timer_on) { timer_on = FALSE; stall_acc = 0; ui_update_timer(0); }

        if (key.ScanCode == SCAN_UP && selected > 0) {
            UINTN old = selected--;
            ui_update_selection(old, cfg.Entries[old].Label, selected, cfg.Entries[selected].Label);
        } else if (key.ScanCode == SCAN_DOWN && selected < cfg.EntryCount - 1) {
            UINTN old = selected++;
            ui_update_selection(old, cfg.Entries[old].Label, selected, cfg.Entries[selected].Label);
        } else if (key.ScanCode == SCAN_HOME && selected) {
            UINTN old = selected; selected = 0;
            ui_update_selection(old, cfg.Entries[old].Label, 0, cfg.Entries[0].Label);
        } else if (key.ScanCode == SCAN_END && selected != cfg.EntryCount-1) {
            UINTN old = selected; selected = cfg.EntryCount-1;
            ui_update_selection(old, cfg.Entries[old].Label, selected, cfg.Entries[selected].Label);
        } else if (key.ScanCode == SCAN_F5) {
            config_load(ImageHandle, ST, &cfg);
            scanner_run(ImageHandle, &cfg);
            if (selected >= cfg.EntryCount) selected = 0;
            ui_full_redraw(); draw_all(&cfg, selected);
            ui_set_status(COL_SUCCESS, L"Rescanned. Entries updated.");
        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            do_boot(ImageHandle, ST, &cfg, selected);
            ui_full_redraw(); draw_all(&cfg, selected);
        }
    }
    return EFI_SUCCESS;
}