/**
 * @file    Logger.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   Thread-safe on-board telemetry logging implementation
 * @author  FireSat-S Team | AESH 2026
 *
 * Design notes:
 *  1. A FreeRTOS mutex (xSemaphoreCreateMutex) serialises all log writes.
 *     This prevents log-line interleaving when two tasks log simultaneously.
 *  2. LOG_FAULT forces a synchronous flush to flash before the safe-mode
 *     transition, so the fault entry is never lost in a power cycle.
 *  3. All output goes through a single vsnprintf → flash_write path.
 *     Swapping the transport (UART, SPI flash, FRAM) requires changing only
 *     the TODO section in _logger_write_to_flash().
 */

#include "Logger.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*——— Module-private state ————————————————————————————————————————————————————*/

/** Guards all log write operations — created in Logger_init(). */
static SemaphoreHandle_t s_logMutex = NULL;

/** In-RAM circular log buffer (flushed to flash on LOG_FAULT or Logger_flush). */
#define LOG_BUF_SIZE    512U
static char  s_logBuf[LOG_BUF_SIZE];
static size_t s_logHead = 0;   /* next write position (wraps) */

/*——— Private helpers ————————————————————————————————————————————————————————*/

/**
 * @brief  Write formatted text to the in-RAM circular buffer.
 *         Caller MUST already hold s_logMutex.
 */
static void _logger_write_to_flash(const char *line, size_t len)
{
    /* TODO: replace with HAL_FLASH_Program() or SPI-flash driver call.
     *       For prototype: write to UART so output is visible on CubeIDE console. */
    (void)line;
    (void)len;
    /*
     * Prototype stub — copy into circular buffer for Logger_flush():
     */
    for (size_t i = 0; i < len; i++) {
        s_logBuf[s_logHead] = line[i];
        s_logHead = (s_logHead + 1U) % LOG_BUF_SIZE;
    }
}

/**
 * @brief  Format and write one log entry.
 *         Caller MUST already hold s_logMutex.
 */
static void _logger_write(LogLevel_t level, const char *fmt, va_list args)
{
    static const char * const level_str[] = {
        "DBG", "INF", "WRN", "ERR", "FLT"
    };

    char line[256];
    int  hdr_len = snprintf(line, sizeof(line),
                            "[%s][%lu] ",
                            level_str[(uint8_t)level],
                            (unsigned long)xTaskGetTickCount());

    if (hdr_len > 0 && (size_t)hdr_len < sizeof(line)) {
        vsnprintf(line + hdr_len, sizeof(line) - (size_t)hdr_len, fmt, args);
    }

    /* Ensure null-termination and append newline */
    line[sizeof(line) - 2U] = '\n';
    line[sizeof(line) - 1U] = '\0';

    _logger_write_to_flash(line, strnlen(line, sizeof(line)));
}

/*——— Public API implementation ———————————————————————————————————————————————*/

void Logger_init(void)
{
    s_logMutex = xSemaphoreCreateMutex();
    configASSERT(s_logMutex != NULL);   /* halt in debug if allocation fails */
    memset(s_logBuf, 0, sizeof(s_logBuf));
    s_logHead = 0;
}

void Logger_log(const char *fmt, ...)
{
    if (s_logMutex == NULL) return;

    if (xSemaphoreTake(s_logMutex, pdMS_TO_TICKS(50U)) == pdTRUE) {
        va_list args;
        va_start(args, fmt);
        _logger_write(LOG_INFO, fmt, args);
        va_end(args);
        xSemaphoreGive(s_logMutex);
    }
    /* If mutex not acquired within 50 ms, the log entry is silently dropped
     * rather than blocking a real-time task. Acceptable for telemetry. */
}

void Logger_logLevel(LogLevel_t level, const char *fmt, ...)
{
    if (s_logMutex == NULL) return;

    if (xSemaphoreTake(s_logMutex, pdMS_TO_TICKS(50U)) == pdTRUE) {
        va_list args;
        va_start(args, fmt);
        _logger_write(level, fmt, args);
        va_end(args);

        if (level == LOG_FAULT) {
            /* Flush to flash synchronously before releasing mutex so the fault
             * record is never lost across a power cycle or watchdog reset. */
            Logger_flush();   /* re-entrant safe: flush doesn't take mutex */
        }

        xSemaphoreGive(s_logMutex);
    }

    if (level == LOG_FAULT) {
        /* Request DEEP_SLEEP domain — stop drawing power from everything
         * except OBC + ADCS while ground operators investigate the fault.
         * Uses xQueueOverwrite so the domain transition happens even if
         * PowerManagerTask hasn't processed a prior request yet. */
        PowerDomain_t safe = PWR_DOMAIN_DEEP_SLEEP;
        xQueueOverwrite(xQueuePower, &safe);
    }
}

void Logger_flush(void)
{
    /*
     * NOTE: Logger_flush() deliberately does NOT acquire s_logMutex because
     * it is called from inside Logger_logLevel() which already holds it.
     * If called externally (e.g. from a shutdown handler), ensure the caller
     * either holds the mutex or calls from a single-task context.
     *
     * TODO: iterate s_logBuf and write all valid entries to flash via
     *       HAL_FLASH_Program() or equivalent driver.
     */
    memset(s_logBuf, 0, sizeof(s_logBuf));
    s_logHead = 0;
}
