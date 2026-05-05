/**
 * @file    SensingTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — MWIR thermal capture + TinyML fire detection
 * @author  FireSat-S Team | AESH 2026
 *
 * This task runs only when the power domain is SENSING or TRANSMIT.
 * It blocks on xQueuePosition waiting for OrbitalTask to confirm we
 * are inside the target geofence. When a fire is detected with
 * confidence > 0.92, it:
 *   - Posts FireAlert_t to xQueueAlert  (→ CommunicationTask for UHF TX)
 *   - Posts IRFrame_t  to xQueueStorage (→ StorageTask for S-band downlink)
 */

#include "OrbitalMechanics.h"
#include "TinyML_FireModel.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Task configuration ───────────────────────────────────────────────────── */
#define POSITION_WAIT_MS    11000U   /* max wait for fresh position from OrbitalTask */
#define SENSOR_RETRY_MAX        3U   /* retries before declaring sensor fault        */

/* ── Task handle ──────────────────────────────────────────────────────────── */
TaskHandle_t xSensingTaskHandle = NULL;

/* ── Forward declarations ─────────────────────────────────────────────────── */
static bool captureFrame(IRFrame_t *frame, const GeoPosition_t *pos);

/* ══════════════════════════════════════════════════════════════════════════ */

void SensingTask(void *pvParameters) {
    (void)pvParameters;

    /* Load TinyML model weights into AI co-processor */
    if (!TinyML_init()) {
        Logger_logLevel(LOG_FAULT, "SensingTask: TinyML model load failed");
        vTaskDelete(NULL);
        return;
    }
    Logger_log("SensingTask: model ready — %s", TinyML_getModelVersion());

    GeoPosition_t pos;

    for (;;) {
        /* ── 1. Block until OrbitalTask provides a fresh position ─────────── */
        if (xQueueReceive(xQueuePosition, &pos, pdMS_TO_TICKS(POSITION_WAIT_MS))
                != pdPASS) {
            Logger_logLevel(LOG_WARNING,
                "SensingTask: position timeout — skipping cycle");
            continue;
        }

        /* ── 2. Only sense when inside target zone ────────────────────────── */
        if (!OrbitalMechanics_isInTargetZone(&pos)) {
            /* Outside zone — OrbitalTask already set domain to DEEP_SLEEP.
               This task has nothing to do until the next overpass.           */
            continue;
        }

        /* ── 3. Capture thermal frame (with retry) ────────────────────────── */
        IRFrame_t frame = {};
        bool captured = false;

        for (uint8_t attempt = 0; attempt < SENSOR_RETRY_MAX; attempt++) {
            if (captureFrame(&frame, &pos)) {
                captured = true;
                break;
            }
            Logger_logLevel(LOG_WARNING,
                "SensingTask: capture failed (attempt %d/%d)",
                attempt + 1, SENSOR_RETRY_MAX);
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        if (!captured) {
            Logger_logLevel(LOG_ERROR,
                "SensingTask: all capture attempts failed — sensor fault");
            continue;
        }

        /* ── 4. Run AI inference ──────────────────────────────────────────── */
        FireAlert_t alert = TinyML_runInference(&frame);

        Logger_log("SensingTask: inference done  fire=%d  conf=%.2f  "
                   "lat=%.3f lon=%.3f",
                   (int)alert.fireDetected, alert.confidence,
                   alert.lat, alert.lon);

        /* ── 5. Fire confirmed — route to comms and storage ──────────────── */
        if (alert.fireDetected && alert.confidence > MODEL_CONFIDENCE_THRES) {

            Logger_logLevel(LOG_INFO,
                "SensingTask: FIRE DETECTED  conf=%.2f  area=%.1f ha  "
                "lat=%.4f lon=%.4f",
                alert.confidence, alert.area_ha, alert.lat, alert.lon);

            /* UHF alert — sent immediately (< 200 bytes, comms task handles TX) */
            if (xQueueSend(xQueueAlert, &alert, pdMS_TO_TICKS(500)) != pdPASS) {
                Logger_logLevel(LOG_ERROR,
                    "SensingTask: alert queue full — UHF TX may be delayed");
            }

            /* Full frame to flash storage for later S-band downlink */
            if (xQueueSend(xQueueStorage, &frame, 0) != pdPASS) {
                Logger_logLevel(LOG_WARNING,
                    "SensingTask: storage queue full — frame dropped");
            }
        }
        /* No fire — frame discarded. Comms TX stays off. This is the
           ~85% power saving on communications. */
    }
}

/* ── Private: capture one thermal frame from MWIR imager ─────────────────── */
static bool captureFrame(IRFrame_t *frame, const GeoPosition_t *pos) {
    /* In flight: replace body with actual MWIR driver call.
       For ground prototype: populate with test pattern or UART input.      */
    if (frame == NULL || pos == NULL) return false;

    frame->lat_center  = pos->lat;
    frame->lon_center  = pos->lon;
    frame->timestamp   = pos->timestamp;

    /* TODO: MWIR_captureFrame(frame->pixels) — driver call here */
    return true;
}
