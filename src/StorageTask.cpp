/**
 * @file    StorageTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — on-board flash storage and downlink buffer
 * @author  FireSat-S Team | AESH 2026
 *
 * Stores confirmed-fire IRFrame_t packets to flash for later S-band
 * downlink. Runs at LOW priority — only executes when higher-priority
 * tasks (Orbital, Sensing, Comms) are blocked or sleeping.
 */

#include "TinyML_FireModel.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Flash storage parameters ─────────────────────────────────────────────── */
#define FLASH_PAGE_SIZE_BYTES   4096U
#define MAX_STORED_FRAMES        256U   /* ~256 confirmed fire frames on flash */

TaskHandle_t xStorageTaskHandle = NULL;

static uint32_t framesStored = 0;

/* ══════════════════════════════════════════════════════════════════════════ */

void StorageTask(void *pvParameters) {
    (void)pvParameters;

    Logger_log("StorageTask: ready — flash buffer max %u frames", MAX_STORED_FRAMES);

    IRFrame_t frame;

    for (;;) {
        /* Block indefinitely until SensingTask posts a confirmed-fire frame */
        if (xQueueReceive(xQueueStorage, &frame, portMAX_DELAY) == pdPASS) {

            if (framesStored >= MAX_STORED_FRAMES) {
                Logger_logLevel(LOG_WARNING,
                    "StorageTask: flash buffer full (%u frames) — oldest overwritten",
                    MAX_STORED_FRAMES);
            }

            /* TODO: Flash_writePage(&frame, sizeof(IRFrame_t)) — driver call */
            framesStored++;

            Logger_log("StorageTask: frame stored  total=%lu  "
                       "lat=%.3f lon=%.3f ts=%lu",
                       framesStored, frame.lat_center,
                       frame.lon_center, frame.timestamp);
        }
    }
}
