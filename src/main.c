/* =============================================================================
 * main.c  –  UEFI Bootloader entry point
 *
 * Event loop uses ReadKeyStroke + Stall polling instead of WaitForEvent.
 * WaitForEvent(ConIn->WaitForKey) crashes on some OVMF builds when the
 * WaitForKey event handle is not properly initialised by firmware.
 *
 * Timer is implemented as a manual counter:
 *   every ~1 000 000 µs (1 second) of Stall we decrement the countdown.
 * =============================================================================
 */

#include <efi.h>
#include <efilib.h>
#include "../include/ui.h"
#include "../include/config.h"
#include "../include/boot.h"

/* ── Forward declarations ────────────────────────────────────────────────── */
static void draw_all_menu_rows(const BootConfig *cfg, UINTN selected);
static void run_boot_attempt(EFI_HANDLE ImageHandle,
                             EFI_SYSTEM_TABLE *ST,
                             const BootConfig *cfg,
                             UINTN selected);

/* ── Safe key polling ────────────────────────────────────────────────────── */
/*
 * poll_key() – non-blocking.
 * Returns TRUE and fills *key if a keystroke is available.
 * Returns FALSE immediately if no key is waiting.
 */
static BOOLEAN poll_key(EFI_SYSTEM_TABLE *ST, EFI_INPUT_KEY *key)
{
    EFI_STATUS s = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2,
                                     ST->ConIn, key);
    return (s == EFI_SUCCESS);
}

/* ── efi_main ─────────────────────────────────────────────────────────────── */
EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST)
{
    InitializeLib(ImageHandle, ST);

    /* 1. Init UI – resets console, hides cursor, clears screen exactly once */
    ui_init(ST);

    /* 2. Load config from disk (falls back to built-in defaults if missing) */
    BootConfig cfg;
    config_load(ImageHandle, ST, &cfg);

    if (cfg.EntryCount == 0) {
        ui_set_status(COL_ERROR,
            L"No boot entries found. Add entries to EFI\\BOOT\\bootloader.conf");
        /* Spin until power-off */
        for (;;) uefi_call_wrapper(gBS->Stall, 1, (UINTN)500000);
    }

    /* 3. Draw static chrome ONCE (borders, title, key hints) */
    ui_draw_chrome();

    /* 4. Draw all menu entries */
    UINTN selected = cfg.DefaultIndex;
    draw_all_menu_rows(&cfg, selected);

    /* 5. Countdown timer state */
    UINTN    countdown     = cfg.TimeoutSeconds;
    BOOLEAN  timer_running = (countdown > 0);
    UINTN    stall_budget  = 0;   /* accumulated µs towards next second tick */

    if (timer_running)
        ui_update_timer(countdown);

    /* 6. Main event loop — poll for keys, decrement timer with Stall */
    for (;;) {
        EFI_INPUT_KEY key;

        /* ── Timer tick logic ───────────────────────────────────────────── */
        if (timer_running) {
            /*
             * Stall 10 ms at a time so we stay responsive to keys.
             * After 100 × 10 ms = 1000 ms, fire a timer tick.
             */
            uefi_call_wrapper(gBS->Stall, 1, (UINTN)10000);  /* 10 ms */
            stall_budget += 10000;

            if (stall_budget >= 1000000) {  /* 1 second elapsed */
                stall_budget = 0;
                if (countdown > 0) countdown--;
                ui_update_timer(countdown);

                if (countdown == 0) {
                    timer_running = FALSE;
                    ui_update_timer(0);
                    run_boot_attempt(ImageHandle, ST, &cfg, selected);
                    /* Boot failed – redraw everything and restart timer */
                    ui_full_redraw();
                    draw_all_menu_rows(&cfg, selected);
                    countdown     = cfg.TimeoutSeconds;
                    timer_running = (countdown > 0);
                    stall_budget  = 0;
                    if (timer_running) ui_update_timer(countdown);
                    continue;
                }
            }
        }

        /* ── Key poll (non-blocking) ────────────────────────────────────── */
        /* When timer is off, stall 5ms so we don't hammer ReadKeyStroke
         * at millions of calls/second — that can confuse some OVMF builds. */
        if (!timer_running)
            uefi_call_wrapper(gBS->Stall, 1, (UINTN)5000);

        if (!poll_key(ST, &key)) continue;

        /* Any key cancels the countdown */
        if (timer_running) {
            timer_running = FALSE;
            stall_budget  = 0;
            ui_update_timer(0);
        }

        /* ── Navigation ──────────────────────────────────────────────────── */
        if (key.ScanCode == SCAN_UP && selected > 0) {
            UINTN old = selected--;
            ui_update_selection(old,      cfg.Entries[old].Label,
                                selected, cfg.Entries[selected].Label);

        } else if (key.ScanCode == SCAN_DOWN &&
                   selected < cfg.EntryCount - 1) {
            UINTN old = selected++;
            ui_update_selection(old,      cfg.Entries[old].Label,
                                selected, cfg.Entries[selected].Label);

        } else if (key.ScanCode == SCAN_HOME && selected != 0) {
            UINTN old = selected; selected = 0;
            ui_update_selection(old, cfg.Entries[old].Label,
                                0,   cfg.Entries[0].Label);

        } else if (key.ScanCode == SCAN_END &&
                   selected != cfg.EntryCount - 1) {
            UINTN old = selected;
            selected  = cfg.EntryCount - 1;
            ui_update_selection(old,      cfg.Entries[old].Label,
                                selected, cfg.Entries[selected].Label);

        } else if (key.ScanCode == SCAN_F5) {
            /* Reload config from disk */
            config_load(ImageHandle, ST, &cfg);
            if (selected >= cfg.EntryCount) selected = 0;
            ui_full_redraw();
            draw_all_menu_rows(&cfg, selected);
            ui_set_status(COL_SUCCESS, L"Config reloaded.");

        } else if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            /* Boot selected entry */
            run_boot_attempt(ImageHandle, ST, &cfg, selected);
            /* Only here on failure */
            ui_full_redraw();
            draw_all_menu_rows(&cfg, selected);
        }
        /* All other keys are silently ignored */
    }

    return EFI_SUCCESS;
}

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static void draw_all_menu_rows(const BootConfig *cfg, UINTN selected)
{
    for (UINTN i = 0; i < cfg->EntryCount; i++)
        ui_draw_menu_row(i, cfg->Entries[i].Label, (BOOLEAN)(i == selected));
}

static void run_boot_attempt(EFI_HANDLE       ImageHandle,
                              EFI_SYSTEM_TABLE *ST,
                              const BootConfig *cfg,
                              UINTN             selected)
{
    ui_set_status(COL_HIGHLIGHT, L"Booting — please wait...");

    BootResult r = boot_entry(ImageHandle, ST, &cfg->Entries[selected]);

    /* Only reached on boot failure */
    /* Build "Boot failed: <reason>" without relying on SPrint */
    CHAR16 msg[200];
    UINTN i = 0;
    const CHAR16 *pre = L"Boot failed: ";
    while (*pre && i < 198) msg[i++] = *pre++;
    const CHAR16 *rsn = boot_result_string(r);
    while (*rsn && i < 198) msg[i++] = *rsn++;
    msg[i] = L'\0';

    ui_set_status(COL_ERROR, msg);

    /* Wait for a key before returning to menu */
    EFI_INPUT_KEY discard;
    while (uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2,
                             ST->ConIn, &discard) != EFI_SUCCESS) {
        uefi_call_wrapper(gBS->Stall, 1, (UINTN)10000);
    }
}