/**
 * @file    Queues.h
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS inter-task queue handles (shared across all tasks)
 * @author  FireSat-S Team | AESH 2026
 *
 * @note    All tasks communicate exclusively through these queues —
 *          no shared global variables, no direct function calls between tasks.
 *          This ensures deterministic timing, fault isolation, and scalability.
 *
 * @note    Queue design rationale:
 *          - xQueuePosition uses depth=1 with xQueueOverwrite() — OrbitalTask
 *            always publishes the LATEST position; older values are discarded.
 *            SensingTask reads with xQueuePeek() so the value stays available
 *            for CommunicationTask's groundStationInView() check as well.
 *          - xQueueAlert uses depth=4 to absorb burst detections without
 *            dropping alerts while CommunicationTask is mid-transmission.
 *          - xQueuePower uses depth=2 so OrbitalTask can overwrite a pending
 *            domain request before PowerManagerTask processes it.
 *          - xQueueStorage uses depth=8 to buffer confirmed fire frames
 *            across a full sensing window before StorageTask drains them.
 */

#ifndef QUEUES_H
#define QUEUES_H

#include "FreeRTOS.h"
#include "queue.h"
#include "OrbitalMechanics.h"
#include "TinyML_FireModel.h"
#include "PowerDomain.h"

/*——— Queue depths ———————————————————————————————————————————————————————————*/

/**
 * @brief  Depth = 1: OrbitalTask uses xQueueOverwrite() (latest-value queue).
 *         xQueueOverwrite() is only valid on queues of length 1.
 *         SensingTask reads with xQueuePeek() so the position remains in the
 *         queue for subsequent reads (e.g. CommunicationTask's orbit check).
 *
 * CHANGED from 2 → 1: xQueueOverwrite() is undefined behaviour on depth > 1.
 */
#define Q_DEPTH_POSITION    1U

/**
 * @brief  Depth = 4: absorbs burst fire detections.
 *         SensingTask → CommunicationTask.
 */
#define Q_DEPTH_ALERT       4U

/**
 * @brief  Depth = 2: OrbitalTask may overwrite a pending domain request
 *         before PowerManagerTask acts on it.
 *         OrbitalTask → PowerManagerTask (via PowerManagerTask wrapper).
 */
#define Q_DEPTH_POWER       2U

/**
 * @brief  Depth = 8: buffers confirmed-fire frames across a sensing window.
 *         SensingTask → StorageTask.
 */
#define Q_DEPTH_STORAGE     8U

/*——— Queue handles (defined once in main.cpp, extern everywhere else) ———————*/

/** GeoPosition_t  — current satellite position (latest-value, peek-safe) */
extern QueueHandle_t xQueuePosition;

/** FireAlert_t    — confirmed fire detections */
extern QueueHandle_t xQueueAlert;

/** PowerDomain_t  — power domain requests */
extern QueueHandle_t xQueuePower;

/** IRFrame_t      — confirmed-fire thermal frames to store */
extern QueueHandle_t xQueueStorage;

/*——— Queue initialisation (called once from main.cpp before tasks start) ———*/

/**
 * @brief  Create all inter-task queues.
 *         Must be called after FreeRTOS heap is ready and before
 *         vTaskStartScheduler().
 */
void Queues_init(void);

#endif /* QUEUES_H */
