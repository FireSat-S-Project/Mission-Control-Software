/**
 * @file    CommunicationTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — UHF emergency alerts + S-band image downlink
 * @author  FireSat-S Team | AESH 2026
 *
 * Two communication paths:
 *
 *   UHF (emergency) — 9.6 kbps, < 200 bytes per alert
 *     Activated immediately when SensingTask posts a FireAlert_t.
 *     Gate PIN_GATE_UHF opened for transmission only, closed immediately after.
 *     Latency goal: alert reaches ground within 2 minutes of detection.
 *
 *   S-band (primary) — 10 Mbps, full image frames
 *     Activated only when a ground station is in view (computed from orbital
 *     position). Full IRFrame_t packets from StorageTask are downlinked here.
 *     Gate PIN_GATE_SBAND opened for contact window only.
 */

#include "TinyML_FireModel.h"
#include "OrbitalMechanics.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Task configuration ───────────────────────────────────────────────────── */
#define ALERT_QUEUE_WAIT_MS     500U    /* how long to wait for a new alert   */
#define UHF_TX_TIMEOUT_MS      2000U   /* max time allowed for one UHF TX    */

/* ── Ground station contact windows (simplified — 4 stations worldwide) ───── */
/* In a full implementation these are computed from TLE overpass predictions.  */
#define GS_CONTACT_WINDOW_SEC    600U  /* ~10 min contact per overpass        */

TaskHandle_t xCommunicationTaskHandle = NULL;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static void sendUHFAlert(const FireAlert_t *alert);
static bool groundStationInView(void);

/* ══════════════════════════════════════════════════════════════════════════ */

void CommunicationTask(void *pvParameters) {
    (void)pvParameters;

    Logger_log("CommunicationTask: ready — UHF + S-band");

    FireAlert_t alert;

    for (;;) {
        /* ── 1. Check for fire alerts from SensingTask (UHF path) ─────────── */
        if (xQueueReceive(xQueueAlert, &alert, pdMS_TO_TICKS(ALERT_QUEUE_WAIT_MS))
                == pdPASS) {
            sendUHFAlert(&alert);
        }

        /* ── 2. S-band downlink window — only when ground station is visible ─ */
        if (groundStationInView()) {
            /* Open S-band gate only during contact window */
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_SET);

            Logger_log("CommunicationTask: S-band window open — downlinking");

            /* TODO: drain StorageTask queue and transmit frames via S-band driver */

            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_RESET);
            Logger_log("CommunicationTask: S-band window closed");
        }
    }
}

/* ── Private: transmit emergency fire alert over UHF ─────────────────────── */
static void sendUHFAlert(const FireAlert_t *alert) {
    /* Open UHF gate — 5 W active */
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_UHF, GPIO_PIN_SET);

    Logger_logLevel(LOG_INFO,
        "CommunicationTask: UHF TX  conf=%.2f  lat=%.4f  lon=%.4f  area=%.1f ha",
        alert->confidence, alert->lat, alert->lon, alert->area_ha);

    /* TODO: UHF_transmitAlert(alert) — driver call here
       Packet size < 200 bytes:
         4 bytes  timestamp
         4 bytes  latitude  (float)
         4 bytes  longitude (float)
         4 bytes  confidence (float)
         4 bytes  area_ha   (float)
         1 byte   checksum
       Total: ~21 bytes + framing = well under 200 bytes               */

    vTaskDelay(pdMS_TO_TICKS(UHF_TX_TIMEOUT_MS));   /* allow TX to complete */

    /* Close UHF gate immediately — power gate is open only during TX */
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_UHF, GPIO_PIN_RESET);

    Logger_log("CommunicationTask: UHF gate closed after TX");
}

/* ── Private: check if any ground station is currently in view ───────────── */
static bool groundStationInView(void) {
    GeoPosition_t pos;
    if (xQueuePeek(xQueuePosition, &pos, 0) != pdPASS) return false;

    /* Simplified elevation check — 4 ground stations worldwide.
       Full implementation: compute elevation angle from TLE + station coords */
    const float gs_lats[] = { 36.8f, 51.5f, -33.9f, 43.6f };  /* DZ,UK,AU,CA */
    const float gs_lons[] = {  3.1f,  -0.1f, 151.2f,-79.4f };

    for (int i = 0; i < 4; i++) {
        float dlat = pos.lat - gs_lats[i];
        float dlon = pos.lon - gs_lons[i];
        float dist2 = dlat*dlat + dlon*dlon;
        if (dist2 < (15.0f * 15.0f)) return true;   /* ~15 deg footprint      */
    }
    return false;
}
