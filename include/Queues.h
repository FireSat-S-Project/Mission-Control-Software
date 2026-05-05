#ifndef QUEUES_H
#define QUEUES_H

#include "FreeRTOS.h"
#include "queue.h"

/**
 * Global Queue Handles for Inter-Task Communication (ITC)
 */
extern QueueHandle_t positionQueue; 
extern QueueHandle_t alertQueue;    
extern QueueHandle_t powerQueue;    

#endif
