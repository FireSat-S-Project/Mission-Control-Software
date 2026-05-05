/**
 * @file    Logger.h
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   On-board telemetry logging and fault reporting API
 * @author  FireSat-S Team | AESH 2026
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "FreeRTOS.h"

/* ── Log levels ───────────────────────────────────────────────────────────── */
typedef enum : uint8_t {
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARNING = 2,
    LOG_ERROR   = 3,
    LOG_FAULT   = 4    /* triggers safe-mode transition */
} LogLevel_t;

/* ── Public API ───────────────────────────────────────────────────────────── */
void Logger_init(void);
void Logger_log(const char *fmt, ...);
void Logger_logLevel(LogLevel_t level, const char *fmt, ...);
void Logger_flush(void);   /* write buffered logs to flash before power-down */

#endif /* LOGGER_H */
