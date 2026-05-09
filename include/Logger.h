/**
 * @file    Logger.h
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   On-board telemetry logging and fault reporting API
 * @author  FireSat-S Team | AESH 2026
 *
 * @note    Thread safety:
 *          Logger_init() creates a FreeRTOS mutex internally.
 *          All public API functions (Logger_log, Logger_logLevel, Logger_flush)
 *          acquire the mutex before writing and release it after, making the
 *          Logger safe to call from any task simultaneously.
 *
 *          Do NOT call Logger_log() from an ISR — mutex acquisition blocks
 *          and is not valid inside an interrupt context.
 *          Use a dedicated ISR-safe logging queue if ISR logging is needed.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"   /* FreeRTOS mutex / semaphore */

/*——— Log levels ——————————————————————————————————————————————————————————————*/

typedef enum : uint8_t {
    LOG_DEBUG   = 0,
    LOG_INFO    = 1,
    LOG_WARNING = 2,
    LOG_ERROR   = 3,
    LOG_FAULT   = 4    /**< Triggers autonomous safe-mode transition */
} LogLevel_t;

/*——— Public API ——————————————————————————————————————————————————————————————*/

/**
 * @brief  Initialise the logger subsystem.
 *         Creates the internal mutex and prepares the flash write buffer.
 *         MUST be called from main() before any task starts.
 */
void Logger_init(void);

/**
 * @brief  Log a message at the default INFO level (printf-style).
 *         Thread-safe: acquires internal mutex before writing.
 *
 * @param  fmt  printf-format string
 * @param  ...  variadic arguments
 */
void Logger_log(const char *fmt, ...);

/**
 * @brief  Log a message at an explicit level (printf-style).
 *         Thread-safe: acquires internal mutex before writing.
 *         LOG_FAULT triggers an immediate safe-mode domain transition.
 *
 * @param  level  LogLevel_t severity
 * @param  fmt    printf-format string
 * @param  ...    variadic arguments
 */
void Logger_logLevel(LogLevel_t level, const char *fmt, ...);

/**
 * @brief  Flush the in-RAM log buffer to flash storage.
 *         Call before any planned power-down or reboot.
 *         Thread-safe: acquires internal mutex before flushing.
 */
void Logger_flush(void);

#endif /* LOGGER_H */
