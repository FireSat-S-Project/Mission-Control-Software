/**
 * @file    PowerManager.cpp
 * @project FireSat-S — Sustainable Wildfire Detection Satellite
 * @brief   Hardware-level MOSFET power domain gating
 * @author  FireSat-S Team | AESH 2026
 *
 * Design principles:
 *   1. Fail-safe  — ALL gates close before ANY gate opens (no shoot-through)
 *   2. HAL-based  — STM32 HAL only, no Arduino.h
 *   3. RTOS-aware — vTaskDelay for cooler warm-up (never busy-wait)
 *   4. Logged     — every transition written to flash telemetry
 *
 * Power savings verified against docs/Power_Analysis.xlsx:
 *   Average power WITH gating : 22.5 W  (79.3% saving vs. always-on 109 W)
 *   Energy per orbit WITH gating: 36.8 Wh vs. 178.0 Wh without
 */

#include "PowerDomain.h"
#include "Logger.h"
#include "FreeRTOS.h"
#include "task.h"

/* ── Module-private state ─────────────────────────────────────────────────── */
static PowerDomain_t currentDomain = PWR_DOMAIN_DEEP_SLEEP;

/* ══════════════════════════════════════════════════════════════════════════ */

void PowerManager_init(void) {
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

/* ══════════════════════════════════════════════════════════════════════════ */

void PowerManager_setDomain(PowerDomain_t domain) {
    if (domain == currentDomain) return;   /* no transition needed */

    /* ── Step 1: ALWAYS close every gate first ──────────────────────────────
       This is the critical fail-safe step.
       If the MCU resets mid-transition, all gates default LOW — never stuck
       in a high-power state that would drain or damage hardware.           */
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_ALL, GPIO_PIN_RESET);

    /* ── Step 2: Enable only what this domain requires ──────────────────── */
    switch (domain) {

        case PWR_DOMAIN_TRANSMIT:
            /* Transmit needs S-band TX on top of sensing hardware */
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_SET);
            /* fallthrough intentional — TRANSMIT is a superset of SENSING */
            [[fallthrough]];

        case PWR_DOMAIN_SENSING:
            /* Open MWIR imager and AI co-processor gates */
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_MWIR, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_AI,   GPIO_PIN_SET);
            /* Wait for Stirling cooler to reach operating temp (77 K) */
            vTaskDelay(pdMS_TO_TICKS(MWIR_COOLER_STABILIZE_MS));
            break;

        case PWR_DOMAIN_STANDBY:
            /* Core systems (OBC + ADCS) stay alive — no payload gates needed */
            break;

        case PWR_DOMAIN_DEEP_SLEEP:
        default:
            /* All gates already LOW from Step 1 — nothing more to do */
            break;
    }

    /* ── Step 3: Record transition ───────────────────────────────────────── */
    currentDomain = domain;
    Logger_logLevel(LOG_INFO,
        "PowerManager: -> domain %d  current=%.1f W  "
        "(orbit avg=22.5 W, saving=79.3%%)",
        (int)domain,
        PowerManager_getCurrentPower_W());
}

/* ══════════════════════════════════════════════════════════════════════════ */

PowerDomain_t PowerManager_getCurrentDomain(void) {
    return currentDomain;
}

float PowerManager_getCurrentPower_W(void) {
    switch (currentDomain) {
        case PWR_DOMAIN_DEEP_SLEEP: return PWR_DEEP_SLEEP_W;   /* 15.0 W */
        case PWR_DOMAIN_STANDBY:    return PWR_STANDBY_W;      /* 30.0 W */
        case PWR_DOMAIN_SENSING:    return PWR_SENSING_W;      /* 55.0 W */
        case PWR_DOMAIN_TRANSMIT:   return PWR_TRANSMIT_W;     /* 75.0 W */
        default:                    return PWR_DEEP_SLEEP_W;
    }
}

    
