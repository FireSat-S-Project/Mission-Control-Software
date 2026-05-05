#ifndef QUEUES_H
#define QUEUES_H

#include "FreeRTOS.h"
#include "queue.h"

/**
 * Global Queue Handles for Inter-Task Communication (ITC)
 * These allow different tasks to exchange data without race conditions.
 */
extern QueueHandle_t positionQueue; // Stores GeoPosition_t (Lat, Lon)
extern QueueHandle_t alertQueue;    // Stores FireAlert_t data
extern QueueHandle_t powerQueue;    // Stores PowerCommand_t (PWR Modes)

#endif
