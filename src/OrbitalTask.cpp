/**
 * @file    OrbitalTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — SGP4 position propagation + geofence logic
 * @author  FireSat-S Team | AESH 2026
 *
 * This task runs at HIGHEST priority (above SensingTask) because the
 * power gating decision depends entirely on whether we are over the
 * target zone. A stale position = wrong power state = wasted energy or
 * missed fire detection.
 *
 * Execution cycle (every 10 seconds):
 *   1. Propagate satellite position via SGP4 from stored TLE
 *   2. Check if position is inside Algeria / Tunisia geofence
 *   3. Post required PowerDomain_t to xQueuePower
 *   4. Post GeoPosition_t to xQueuePosition (for SensingTask)
 *   5. Pre-warm MWIR cooler if zone entry is < 30 seconds away
 */

#include "OrbitalMechanics.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Task configuration ───────────────────────────────────────────────────── */
#define ORBITAL_TASK_PERIOD_MS    10000U   /* position update interval        */
#define PREWARM_THRESHOLD_SEC        30U   /* start cooler this far before zone*/

/* ── Task handle (extern declared in main.cpp) ───────────────────────────── */
TaskHandle_t xOrbitalTaskHandle = NULL;

/* ══════════════════════════════════════════════════════════════════════════ */

void OrbitalTask(void *pvParameters) {
    (void)pvParameters;

    /* Initialise SGP4 propagator with stored TLE */
    if (!OrbitalMechanics_init()) {
        Logger_logLevel(LOG_FAULT, "OrbitalTask: SGP4 init failed — TLE missing");
        /* Enter safe loop: publish DEEP_SLEEP domain, wait for TLE uplink */
        for (;;) {
            PowerDomain_t safe = PWR_DOMAIN_DEEP_SLEEP;
            xQueueOverwrite(xQueuePower, &safe);
            vTaskDelay(pdMS_TO_TICKS(ORBITAL_TASK_PERIOD_MS));
        }
    }

    Logger_log("OrbitalTask: SGP4 ready — orbit %.0f km SSO %.1f deg",
               ORBIT_ALTITUDE_KM, ORBIT_INCLINATION_DEG);

    for (;;) {
        /* ── 1. Compute current position ──────────────────────────────────── */
        GeoPosition_t pos = OrbitalMechanics_getCurrentPosition();

        if (!pos.tle_valid) {
            Logger_logLevel(LOG_WARNING,
                "OrbitalTask: TLE age > 72 h — position accuracy degraded");
        }

        /* ── 2. Publish position to SensingTask ───────────────────────────── */
        xQueueOverwrite(xQueuePosition, &pos);

        /* ── 3. Geofence decision → power domain request ─────────────────── */
        PowerDomain_t requestedDomain;

        if (OrbitalMechanics_isInTargetZone(&pos)) {
            requestedDomain = PWR_DOMAIN_SENSING;
            Logger_log("OrbitalTask: IN ZONE  lat=%.2f lon=%.2f -> SENSING",
                       pos.lat, pos.lon);
        } else {
            /* ── 4. Pre-warm check — open cooler early if zone is close ───── */
            uint32_t secs = OrbitalMechanics_secondsToNextPass();
            if (secs <= PREWARM_THRESHOLD_SEC) {
                requestedDomain = PWR_DOMAIN_SENSING;   /* pre-warm cooler    */
                Logger_log("OrbitalTask: pre-warm — zone in %lu s", secs);
            } else {
                requestedDomain = PWR_DOMAIN_DEEP_SLEEP;
                Logger_log("OrbitalTask: outside zone — next pass in %lu s, "
                           "domain=DEEP_SLEEP  (saving %.1f%%)",
                           secs, POWER_SAVING_PCT);
            }
        }

        /* ── 5. Send domain request to PowerManager ───────────────────────── */
        xQueueOverwrite(xQueuePower, &requestedDomain);

        /* ── 6. Sleep until next update ───────────────────────────────────── */
        vTaskDelay(pdMS_TO_TICKS(ORBITAL_TASK_PERIOD_MS));
    }
}
