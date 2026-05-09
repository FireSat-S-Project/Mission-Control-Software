/**
 * @file    SensingTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — MWIR thermal capture + TinyML fire detection
 * @author  FireSat-S Team | AESH 2026
 *
 * Execution overview (per cycle):
 *  1. PEEK OrbitalTask's latest GeoPosition_t from xQueuePosition.
 *     Uses xQueuePeek() — NOT xQueueReceive() — so the position stays in the
 *     queue for CommunicationTask's groundStationInView() check.
 *  2. Skip cycle if position is outside target geofence (DEEP_SLEEP domain
 *     was already set by OrbitalTask — nothing to do).
 *  3. Capture one MWIR thermal frame from the imager (with retry).
 *  4. Run TinyML CNN inference on the frame.
 *  5. If fire confirmed (confidence > MODEL_CONFIDENCE_THRES):
 *       a. Post FireAlert_t to xQueueAlert  → CommunicationTask (UHF TX).
 *       b. Post IRFrame_t  to xQueueStorage → StorageTask (S-band downlink).
 *     If no fire: frame discarded, comms TX stays off (~85% power saving on
 *     communications across passes where fire is absent).
 *
 * Priority: HIGH (3) — time-critical; capture window is short.
 */

#include "OrbitalMechanics.h"
#include "TinyML_FireModel.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/*——— Task configuration ——————————————————————————————————————————————————————*/

/** Max wait for a fresh position update from OrbitalTask (11 s > 10 s period). */
#define POSITION_WAIT_MS    11000U

/** Sensor retries before declaring a sensor fault and skipping the frame. */
#define SENSOR_RETRY_MAX    3U

/*——— Task handle (extern-declared in main.cpp) ———————————————————————————————*/

TaskHandle_t xSensingTaskHandle = NULL;

/*——— Private declarations ————————————————————————————————————————————————————*/

static bool captureFrame(IRFrame_t *frame, const GeoPosition_t *pos);

/*============================================================================*/

void SensingTask(void *pvParameters)
{
    (void)pvParameters;

    /* Load TinyML model weights into AI co-processor SRAM. */
    if (!TinyML_init()) {
        Logger_logLevel(LOG_FAULT, "SensingTask: TinyML model load failed");
        vTaskDelete(NULL);
        return;
    }
    Logger_log("SensingTask: model ready — %s", TinyML_getModelVersion());

    GeoPosition_t pos;

    for (;;) {

        /*——— 1. Wait for OrbitalTask to publish a fresh position ——————————————
         *
         * KEY FIX: use xQueuePeek(), NOT xQueueReceive().
         *
         * xQueueReceive() would consume the item, leaving xQueuePosition empty.
         * CommunicationTask calls xQueuePeek(xQueuePosition, ...) inside
         * groundStationInView() — if SensingTask already drained the queue,
         * that check always returns false and S-band downlinks are silently
         * suppressed.
         *
         * xQueuePeek() copies the value into `pos` without removing it from
         * the queue, so both tasks always see the latest orbital position.
         * OrbitalTask uses xQueueOverwrite() (valid only on depth-1 queues)
         * to replace the stale value each cycle — no accumulation possible.
         *——————————————————————————————————————————————————————————————————————*/
        if (xQueuePeek(xQueuePosition, &pos, pdMS_TO_TICKS(POSITION_WAIT_MS))
                != pdPASS)
        {
            Logger_logLevel(LOG_WARNING,
                            "SensingTask: position timeout — skipping cycle");
            continue;
        }

        /*——— 2. Only sense when inside target geofence ————————————————————————*/
        if (!OrbitalMechanics_isInTargetZone(&pos)) {
            /* Outside zone — OrbitalTask already set domain to DEEP_SLEEP.
             * Nothing to do; block until OrbitalTask publishes a new position. */
            continue;
        }

        /*——— 3. Capture thermal frame (with retry) ————————————————————————————*/
        IRFrame_t frame    = {};
        bool      captured = false;

        for (uint8_t attempt = 0U; attempt < SENSOR_RETRY_MAX; attempt++) {
            if (captureFrame(&frame, &pos)) {
                captured = true;
                break;
            }
            Logger_logLevel(LOG_WARNING,
                            "SensingTask: capture failed (attempt %d/%d)",
                            attempt + 1, SENSOR_RETRY_MAX);
            vTaskDelay(pdMS_TO_TICKS(200U));
        }

        if (!captured) {
            Logger_logLevel(LOG_ERROR,
                            "SensingTask: all capture attempts failed — sensor fault");
            continue;
        }

        /*——— 4. Run AI inference ——————————————————————————————————————————————*/
        FireAlert_t alert = TinyML_runInference(&frame);

        Logger_log("SensingTask: inference done  fire=%d  conf=%.2f "
                   "lat=%.3f lon=%.3f",
                   (int)alert.fireDetected,
                   alert.confidence,
                   alert.lat,
                   alert.lon);

        /*——— 5. Fire confirmed — route to comms and storage ——————————————————*/
        if (alert.fireDetected && alert.confidence > MODEL_CONFIDENCE_THRES) {

            Logger_logLevel(LOG_INFO,
                            "SensingTask: FIRE DETECTED  conf=%.2f  area=%.1f ha  "
                            "lat=%.4f  lon=%.4f",
                            alert.confidence, alert.area_ha,
                            alert.lat, alert.lon);

            /* UHF alert — sent immediately (< 200 bytes, CommunicationTask TX) */
            if (xQueueSend(xQueueAlert, &alert, pdMS_TO_TICKS(500U)) != pdPASS) {
                Logger_logLevel(LOG_ERROR,
                                "SensingTask: alert queue full — UHF TX may be delayed");
            }

            /* Full frame to flash storage for later S-band downlink */
            if (xQueueSend(xQueueStorage, &frame, 0U) != pdPASS) {
                Logger_logLevel(LOG_WARNING,
                                "SensingTask: storage queue full — frame dropped");
            }

        } else {
            /* No fire — frame discarded.
             * Comms TX stays off. This is the ~85% power saving on comms
             * across the ~85% of target passes where no fire is present. */
        }

    } /* for (;;) */
}

/*——— Private: capture one thermal frame from MWIR imager ———————————————————*/

/**
 * @brief  Capture one raw MWIR frame and tag it with the current position.
 *
 * @param  frame  Pointer to caller-allocated IRFrame_t to fill.
 * @param  pos    Current satellite GeoPosition_t for geo-tagging.
 * @return true on success, false on sensor timeout / read error.
 *
 * @note   In-flight: replace body with actual MWIR HAL driver call.
 *         Prototype:  populate with test pattern or UART-injected data.
 */
static bool captureFrame(IRFrame_t *frame, const GeoPosition_t *pos)
{
    if (frame == NULL || pos == NULL) return false;

    frame->lat_center  = pos->lat;
    frame->lon_center  = pos->lon;
    frame->timestamp   = pos->timestamp;

    /* TODO: MWIR_captureFrame(frame->pixels) — driver call here */

    return true;
}
