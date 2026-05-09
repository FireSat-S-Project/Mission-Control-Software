/**
 * @file    CommunicationTask.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS task — UHF emergency alerts + S-band image downlink
 * @author  FireSat-S Team | AESH 2026
 *
 * Two communication paths:
 *
 *  UHF (emergency) — 9.6 kbps, < 200 bytes per alert
 *   - Activated immediately when SensingTask posts a FireAlert_t.
 *   - Gate PIN_GATE_UHF opened for transmission only, closed immediately after.
 *   - Latency goal: alert reaches ground within 2 minutes of detection.
 *   - Packet format: 4B timestamp + 4B lat + 4B lon + 4B confidence +
 *                    4B area_ha + 1B checksum = ~21 bytes + framing.
 *
 *  S-band (primary) — 10 Mbps, full image frames
 *   - Activated only when a ground station is in view (computed from orbital
 *     position via xQueuePeek on xQueuePosition).
 *   - Full IRFrame_t packets from StorageTask are downlinked here.
 *   - Gate PIN_GATE_SBAND opened for contact window only.
 *
 * Ground station visibility:
 *   Simplified elevation check against 4 stations worldwide:
 *   DZ (Algeria), UK, AU (Australia), CA (Canada).
 *   Uses squared-distance approximation (~15° footprint at 700 km).
 *
 *   NOTE: This is a prototype approximation acceptable for academic use.
 *   A flight-qualified implementation must use:
 *     - Elevation angle computed from TLE + station geodetic coords
 *     - Earth curvature correction
 *     - Antenna mask model
 *   These are planned as future work.
 *
 * Priority: MEDIUM (2) — alert TX after detection, not time-critical.
 */

#include "TinyML_FireModel.h"
#include "OrbitalMechanics.h"
#include "PowerDomain.h"
#include "Queues.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/*——— Task configuration ——————————————————————————————————————————————————————*/

/** How long to wait for a new fire alert before checking S-band window. */
#define ALERT_QUEUE_WAIT_MS     500U

/** Max time allowed for one UHF transmission (driver timeout). */
#define UHF_TX_TIMEOUT_MS      2000U

/**
 * @brief  Approximate ground station contact window.
 *         In a full implementation these are computed from TLE overpass
 *         predictions per station. ~10 min contact per overpass.
 */
#define GS_CONTACT_WINDOW_SEC   600U

/*——— Task handle (extern-declared in main.cpp) ———————————————————————————————*/

TaskHandle_t xCommunicationTaskHandle = NULL;

/*——— Private declarations ————————————————————————————————————————————————————*/

static void sendUHFAlert(const FireAlert_t *alert);
static bool groundStationInView(void);

/*============================================================================*/

void CommunicationTask(void *pvParameters)
{
    (void)pvParameters;

    Logger_log("CommunicationTask: ready — UHF + S-band");

    FireAlert_t alert;

    for (;;) {

        /*——— 1. Check for fire alerts from SensingTask (UHF path) ————————————*/
        if (xQueueReceive(xQueueAlert, &alert,
                          pdMS_TO_TICKS(ALERT_QUEUE_WAIT_MS)) == pdPASS)
        {
            sendUHFAlert(&alert);
        }

        /*——— 2. S-band downlink window — only when ground station is visible —*/
        if (groundStationInView()) {

            /* Open S-band gate only during contact window */
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_SET);
            Logger_log("CommunicationTask: S-band window open — downlinking");

            /* TODO: drain xQueueStorage and transmit frames via S-band driver.
             *
             *  Suggested implementation:
             *    IRFrame_t frame;
             *    while (xQueueReceive(xQueueStorage, &frame, 0) == pdPASS) {
             *        SBAND_transmitFrame(&frame);
             *    }
             *
             *  The S-band driver (Syrlinks EWC27 or equivalent) handles
             *  framing, encoding, and modulation at 10 Mbps.
             */

            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_RESET);
            Logger_log("CommunicationTask: S-band window closed");
        }

    } /* for (;;) */
}

/*——— Private: transmit emergency fire alert over UHF ————————————————————————*/

/**
 * @brief  Transmit a FireAlert_t over UHF emergency channel.
 *
 *         Packet layout (< 200 bytes, well under UHF MTU):
 *           4 bytes  timestamp    (uint32_t, Unix time)
 *           4 bytes  latitude     (float)
 *           4 bytes  longitude    (float)
 *           4 bytes  confidence   (float)
 *           4 bytes  area_ha      (float)
 *           1 byte   checksum     (XOR of all preceding bytes)
 *           ~framing overhead
 *           Total: ~21 bytes + framing = well under 200 bytes
 *
 * @param  alert  Pointer to confirmed FireAlert_t from SensingTask.
 */
static void sendUHFAlert(const FireAlert_t *alert)
{
    /* Open UHF gate — 5 W active */
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_UHF, GPIO_PIN_SET);

    Logger_logLevel(LOG_INFO,
                    "CommunicationTask: UHF TX  conf=%.2f  lat=%.4f  lon=%.4f  "
                    "area=%.1f ha",
                    alert->confidence,
                    alert->lat,
                    alert->lon,
                    alert->area_ha);

    /* TODO: UHF_transmitAlert(alert) — driver call here.
     *
     * Build packet manually for now (driver stub):
     *
     *   uint8_t pkt[21];
     *   memcpy(pkt + 0,  &alert->timestamp,  4);
     *   memcpy(pkt + 4,  &alert->lat,        4);
     *   memcpy(pkt + 8,  &alert->lon,        4);
     *   memcpy(pkt + 12, &alert->confidence, 4);
     *   memcpy(pkt + 16, &alert->area_ha,    4);
     *   uint8_t csum = 0;
     *   for (int i = 0; i < 20; i++) csum ^= pkt[i];
     *   pkt[20] = csum;
     *   AstroDev_Li2_transmit(pkt, sizeof(pkt));
     */

    /* Allow TX to complete before closing gate */
    vTaskDelay(pdMS_TO_TICKS(UHF_TX_TIMEOUT_MS));

    /* Close UHF gate immediately — power gate is open only during TX */
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_UHF, GPIO_PIN_RESET);
    Logger_log("CommunicationTask: UHF gate closed after TX");
}

/*——— Private: check if any ground station is currently in view ———————————————
 *
 * Simplified elevation check — 4 ground stations worldwide.
 * Uses squared lat/lon distance as a proxy for elevation angle.
 * ~15° footprint corresponds to ~1670 km ground track at 700 km altitude.
 *
 * Ground station coordinates (geodetic):
 *   DZ  — Algiers,    Algeria    (36.8°N,  3.1°E)
 *   UK  — Guildford,  England    (51.5°N, -0.1°E)
 *   AU  — Canberra,   Australia  (-33.9°N, 151.2°E)
 *   CA  — Ottawa,     Canada     (43.6°N, -79.4°E)
 *
 * NOTE: This is an intentional simplification for prototype use.
 * A flight-qualified implementation must compute true elevation angles
 * using station geodetic coordinates and the satellite's ECI position
 * derived from the TLE propagator.
 *————————————————————————————————————————————————————————————————————————————*/

static bool groundStationInView(void)
{
    GeoPosition_t pos;

    /* Use xQueuePeek — does not consume the position from the queue.
     * OrbitalTask continuously overwrites xQueuePosition; both SensingTask
     * and CommunicationTask read the same latest value via peek. */
    if (xQueuePeek(xQueuePosition, &pos, 0) != pdPASS) {
        return false;   /* no position available yet */
    }

    /* Ground station lat/lon table */
    static const float gs_lats[] = {  36.8f,  51.5f, -33.9f,  43.6f };
    static const float gs_lons[] = {   3.1f,  -0.1f,  151.2f, -79.4f };
    static const int   gs_count  = 4;

    for (int i = 0; i < gs_count; i++) {
        float dlat  = pos.lat - gs_lats[i];
        float dlon  = pos.lon - gs_lons[i];
        float dist2 = dlat * dlat + dlon * dlon;

        /* ~15° footprint at 700 km altitude (simplified flat-Earth check).
         * Flight model: replace with elevation_angle(pos, gs_lats[i], gs_lons[i])
         * and check elevation_angle > 5.0f (minimum viable link budget). */
        if (dist2 < (15.0f * 15.0f)) {
            return true;
        }
    }

    return false;
}
