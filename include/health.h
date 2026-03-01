#ifndef HEALTH_H
#define HEALTH_H

#include <efi.h>
#include <efilib.h>
#include "config.h"

typedef enum {
    HEALTH_OK      = 0,
    HEALTH_WARN    = 1,
    HEALTH_FAIL    = 2,
} HealthStatus;

typedef struct {
    char         label[64];
    HealthStatus status;
    char         detail[128];
} HealthCheck;

#define MAX_CHECKS 8

typedef struct {
    HealthCheck checks[MAX_CHECKS];
    UINTN       count;
    HealthStatus overall;
} HealthReport;

/* Run all health checks and fill report */
void health_run(EFI_HANDLE ImageHandle, BootConfig *cfg, HealthReport *report);

#endif