#include "FreeRTOS.h"
#include "task.h"
#include "Queues.h"

/**
 * Define the actual Queue handles
 * (Notice: no 'extern' here, this is the actual allocation)
 */
QueueHandle_t positionQueue;
QueueHandle_t alertQueue;
QueueHandle_t powerQueue;

int main(void) {
    // 1. Create the Queues for inter-task communication
    positionQueue = xQueueCreate(1, sizeof(GeoPosition_t));
    alertQueue    = xQueueCreate(5, sizeof(FireAlert_t));
    powerQueue    = xQueueCreate(5, sizeof(PowerCommand_t));

    // 2. Start System Tasks
    xTaskCreate(PowerManagerTask, "Power_Task", 512, NULL, 4, NULL); 
    xTaskCreate(SensingTask, "Sensing_Task", 1024, NULL, 3, NULL); 
    
    // 3. Start RTOS Scheduler
    vTaskStartScheduler();

    while (1); 
}
