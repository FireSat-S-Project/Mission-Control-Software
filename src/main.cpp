/**
 * @file    main.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS entry point — task creation, queue init, scheduler start
 * @author  FireSat-S Team | AESH 2026
 *
 * Task priority table (higher number = higher priority):
 *
 *   Task               Priority   Stack    Reason
 *   ─────────────────────────────────────────────────────────────────────
 *   PowerManagerTask   5 (HIGH)   512 B    Power state drives everything
 *   OrbitalTask        4 (HIGH)   1024 B   Position determines power state
 *   SensingTask        3 (HIGH)   2048 B   Time-critical — capture window
 *   CommunicationTask  2 (MED)    1024 B   Alert TX after detection
 *   StorageTask        1 (LOW)    512 B    Background flash writes
 *
 * Power saving summary (from docs/Power_Analysis.xlsx):
 *   Without gating : 178.0 Wh/orbit  (109 W constant)
 *   With gating    :  36.8 Wh/orbit  (22.5 W average)
 *   Saving         :  79.3%
 */

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "Logger.h"

/* ── External task functions (defined in their own .cpp files) ───────────── */
extern void OrbitalTask       (void *pvParameters);
extern void SensingTask       (void *pvParameters);
extern void CommunicationTask (void *pvParameters);
extern void StorageTask       (void *pvParameters);

/* ── External task handles ───────────────────────────────────────────────── */
extern TaskHandle_t xOrbitalTaskHandle;
extern TaskHandle_t xSensingTaskHandle;
extern TaskHandle_t xCommunicationTaskHandle;
extern TaskHandle_t xStorageTaskHandle;

/* ── PowerManager task wrapper ───────────────────────────────────────────── */
static void PowerManagerTask(void *pvParameters) {
    (void)pvParameters;
    PowerDomain_t requestedDomain;

    for (;;) {
        /* Block until OrbitalTask sends a domain request */
        if (xQueueReceive(xQueuePower, &requestedDomain, portMAX_DELAY) == pdPASS) {
            PowerManager_setDomain(requestedDomain);
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    /* ── 1. STM32 HAL init ────────────────────────────────────────────────── */
    HAL_Init();

    /* ── 2. Logger + Power Manager (before any tasks run) ────────────────── */
    Logger_init();
    PowerManager_init();   /* forces all MOSFET gates LOW — safe state        */

    Logger_log("FireSat-S boot — AESH 2026");
    Logger_log("Power budget: %.1f Wh/orbit (%.1f%% saving vs. no-gating)",
               ENERGY_GATED_Wh, POWER_SAVING_PCT);

    /* ── 3. Create inter-task queues ──────────────────────────────────────── */
    Queues_init();

    /* ── 4. Create FreeRTOS tasks ─────────────────────────────────────────── */
    xTaskCreate(PowerManagerTask,  "PowerMgr",  512,  NULL, 5, NULL);
    xTaskCreate(OrbitalTask,       "Orbital",   1024, NULL, 4, &xOrbitalTaskHandle);
    xTaskCreate(SensingTask,       "Sensing",   2048, NULL, 3, &xSensingTaskHandle);
    xTaskCreate(CommunicationTask, "Comms",     1024, NULL, 2, &xCommunicationTaskHandle);
    xTaskCreate(StorageTask,       "Storage",   512,  NULL, 1, &xStorageTaskHandle);

    Logger_log("All tasks created — starting FreeRTOS scheduler");

    /* ── 5. Start scheduler — never returns ──────────────────────────────── */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;) {}
    return 0;
}
