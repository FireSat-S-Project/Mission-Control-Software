/**
 * @file    PowerManager.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   Hardware-level MOSFET power domain gating
 * @author  FireSat-S Team | AESH 2026
 *
 * Design principles:
 *  1. Fail-safe   — ALL gates close before ANY gate opens (no shoot-through).
 *  2. HAL-based   — STM32 HAL only; no Arduino.h.
 *  3. RTOS-aware  — vTaskDelay for cooler warm-up lives in PowerManagerTask,
 *                   NOT inside PowerManager_setDomain().  setDomain() is a
 *                   pure state-transition function that must return promptly
 *                   so the caller task is not unnecessarily blocked.
 *  4. Logged      — every domain transition written to flash telemetry.
 *
 * Power savings (verified against docs/Power_Analysis.xlsx):
 *   Average power WITH gating : 22.5 W  (79.3% saving vs always-on 109 W)
 *   Energy per orbit WITH gating: 36.8 Wh vs 178.0 Wh without
 */

#include "PowerDomain.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"
#include "Queues.h"

/*——— Module-private state ————————————————————————————————————————————————————*/

static PowerDomain_t currentDomain = PWR_DOMAIN_DEEP_SLEEP;

/*============================================================================
 *  PowerManager_init
 *===========================================================================*/

void PowerManager_init(void)
{
    /* Configure all gate pins as push-pull outputs, no pull resistor */
    GPIO_InitTypeDef cfg = {};
    cfg.Pin   = PIN_GATE_ALL;
    cfg.Mode  = GPIO_MODE_OUTPUT_PP;
    cfg.Pull  = GPIO_NOPULL;
    cfg.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GATE_PORT, &cfg);

    /* Fail-safe: force all gates LOW before anything else runs */
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_ALL, GPIO_PIN_RESET);
    currentDomain = PWR_DOMAIN_DEEP_SLEEP;

    Logger_log("PowerManager: init OK — all gates LOW, domain=DEEP_SLEEP");
}

/*============================================================================
 *  PowerManager_setDomain
 *
 *  Pure state-transition function — returns promptly.
 *  Callers that need a stabilisation delay (e.g. Stirling cooler warm-up)
 *  must implement that delay in their own task context AFTER calling this
 *  function, not by blocking here.
 *
 *  FIXED: removed vTaskDelay(MWIR_COOLER_STABILIZE_MS) from inside this
 *  function.  Previously the 500 ms cooler warm-up blocked PowerManagerTask
 *  for half a second during every SENSING transition, preventing it from
 *  processing any new domain requests (e.g. an urgent DEEP_SLEEP from a
 *  LOG_FAULT event) during that window.
 *  The delay now lives in PowerManagerTask (see main.cpp) immediately after
 *  the setDomain() call, inside the task loop where blocking is safe.
 *===========================================================================*/

void PowerManager_setDomain(PowerDomain_t domain)
{
    if (domain == currentDomain) return;   /* no transition needed */

    /*——— Step 1: ALWAYS close every gate first ———————————————————————————————
     *
     * Critical fail-safe step.  If the MCU resets mid-transition, all gates
     * default LOW — never stuck in a high-power state that drains or damages
     * hardware.
     *————————————————————————————————————————————————————————————————————————*/
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_ALL, GPIO_PIN_RESET);

    /*——— Step 2: Enable only what this domain requires ———————————————————————
     *
     * TRANSMIT is a superset of SENSING: fallthrough is intentional —
     * TRANSMIT needs both the imaging hardware (SENSING gates) AND the
     * S-band transmitter.
     *————————————————————————————————————————————————————————————————————————*/
    switch (domain) {
        case PWR_DOMAIN_TRANSMIT:
            /* Transmit needs S-band TX on top of sensing hardware */
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_SET);
            [[fallthrough]];

        case PWR_DOMAIN_SENSING:
            /* Open MWIR imager and AI co-processor gates.
             *
             * NOTE: Stirling cooler requires ~500 ms to reach 77 K operating
             * temperature.  This stabilisation delay must be implemented by
             * the CALLER (PowerManagerTask) AFTER this function returns —
             * not here.  See PowerManagerTask in main.cpp. */
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_MWIR, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_AI,   GPIO_PIN_SET);
            break;

        case PWR_DOMAIN_STANDBY:
            /* Core systems (OBC + ADCS) stay alive — no payload gates needed */
            break;

        case PWR_DOMAIN_DEEP_SLEEP:
        default:
            /* All gates already LOW from Step 1 — nothing more to do */
            break;
    }

    /*——— Step 3: Record transition ———————————————————————————————————————————*/
    currentDomain = domain;

    Logger_logLevel(LOG_INFO,
                    "PowerManager: -> domain %d  current=%.1f W  "
                    "(orbit avg=22.5 W, saving=79.3%%)",
                    (int)domain,
                    PowerManager_getCurrentPower_W());
}

/*============================================================================
 *  Getters
 *===========================================================================*/

PowerDomain_t PowerManager_getCurrentDomain(void)
{
    return currentDomain;
}

float PowerManager_getCurrentPower_W(void)
{
    switch (currentDomain) {
        case PWR_DOMAIN_DEEP_SLEEP:  return PWR_DEEP_SLEEP_W;   /* 15.0 W */
        case PWR_DOMAIN_STANDBY:     return PWR_STANDBY_W;      /* 30.0 W */
        case PWR_DOMAIN_SENSING:     return PWR_SENSING_W;      /* 55.0 W */
        case PWR_DOMAIN_TRANSMIT:    return PWR_TRANSMIT_W;     /* 75.0 W */
        default:                     return PWR_DEEP_SLEEP_W;
    }
}
