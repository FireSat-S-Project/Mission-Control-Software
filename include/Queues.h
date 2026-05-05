/**
 * @file    Queues.h
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   FreeRTOS inter-task queue handles (shared across all tasks)
 * @author  FireSat-S Team | AESH 2026
 *
 * @note    All tasks communicate exclusively through these queues —
 *          no shared global variables, no direct function calls between tasks.
 *          This ensures deterministic timing, fault isolation, and scalability.
 */

#ifndef QUEUES_H
#define QUEUES_H

#include "FreeRTOS.h"
#include "queue.h"
#include "OrbitalMechanics.h"
#include "TinyML_FireModel.h"
#include "PowerDomain.h"

/* ── Queue depths ─────────────────────────────────────────────────────────── */
#define Q_DEPTH_POSITION    2    /* OrbitalTask  → SensingTask               */
#define Q_DEPTH_ALERT       4    /* SensingTask  → CommunicationTask         */
#define Q_DEPTH_POWER       2    /* OrbitalTask  → PowerManager              */
#define Q_DEPTH_STORAGE     8    /* SensingTask  → StorageTask               */

/* ── Queue handles (defined once in main.cpp, extern everywhere else) ─────── */
extern QueueHandle_t xQueuePosition;   /* GeoPosition_t  — current sat pos   */
extern QueueHandle_t xQueueAlert;      /* FireAlert_t    — confirmed fires    */
extern QueueHandle_t xQueuePower;      /* PowerDomain_t  — domain requests   */
extern QueueHandle_t xQueueStorage;    /* IRFrame_t      — frames to store   */

/* ── Queue initialisation (called once from main.cpp) ────────────────────── */
void Queues_init(void);

#endif /* QUEUES_H */
