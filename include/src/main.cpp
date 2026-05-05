#include "FreeRTOS.h"
#include "task.h"
#include "Queues.h"

/**
 * Mission-Control-Software Main Entry Point
 * Orchestrates Task Creation and RTOS Scheduler start.
 */
int main(void) {
    // 1. Initialize Communication Queues
    positionQueue = xQueueCreate(1, sizeof(GeoPosition_t));
    alertQueue    = xQueueCreate(5, sizeof(FireAlert_t));
    powerQueue    = xQueueCreate(5, sizeof(PowerCommand_t));

    // 2. Instantiate System Tasks with Specific Priorities
    // Priority 4 (High): Power management for system stability
    xTaskCreate(PowerManagerTask, "Power_Manager", 512, NULL, 4, NULL); 
    
    // Priority 3 (Medium): AI and Sensing logic
    xTaskCreate(SensingTask, "Sensing_Task", 1024, NULL, 3, NULL); 
    
    // 3. Launch the RTOS Scheduler
    vTaskStartScheduler();

    // The system should never reach this point
    while (1); 
}
