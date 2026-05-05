/**
 * @file    PowerManager.cpp
 * @brief   Hardware-level MOSFET power gating — FireSat-S
 *
 * Design principles:
 *  1. Fail-safe: ALL gates close before ANY gate opens (prevents shoot-through)
 *  2. HAL-based: no Arduino.h — uses STM32 HAL for portability
 *  3. RTOS-aware: uses vTaskDelay (never busy-wait)
 *  4. State tracking: currentDomain always reflects hardware state
 */

#include "PowerDomain.h"
#include "Logger.h"

// ── Module state ─────────────────────────────────────────────────────────────
static PowerDomain_t currentDomain = PWR_DOMAIN_DEEP_SLEEP;

// ── Init ─────────────────────────────────────────────────────────────────────
void PowerManager_init(void) {
    GPIO_InitTypeDef cfg = {
        .Pin   = PIN_GATE_ALL,
        .Mode  = GPIO_MODE_OUTPUT_PP,
        .Pull  = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW
    };
    HAL_GPIO_Init(GATE_PORT, &cfg);

    // Fail-safe default: all gates LOW (off)
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_ALL, GPIO_PIN_RESET);
    currentDomain = PWR_DOMAIN_DEEP_SLEEP;

    Logger_log("PowerManager: init — all gates LOW (safe state)");
}

// ── Domain transition ─────────────────────────────────────────────────────────
void PowerManager_setDomain(PowerDomain_t domain) {
    if (domain == currentDomain) return;

    // ── Step 1: ALWAYS close all gates first (fail-safe) ────────────────────
    HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_ALL, GPIO_PIN_RESET);

    // ── Step 2: Open only what this domain needs ─────────────────────────────
    switch (domain) {

        case PWR_DOMAIN_TRANSMIT:
            // Transmit = Sensing + comms TX
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_SBAND, GPIO_PIN_SET);
            /* fallthrough — also needs MWIR + AI */

        case PWR_DOMAIN_SENSING:
            // Open MWIR + AI, wait for Stirling cooler to stabilize
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_MWIR, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GATE_PORT, PIN_GATE_AI,   GPIO_PIN_SET);
            vTaskDelay(pdMS_TO_TICKS(MWIR_COOLER_STABILIZE_MS));
            break;

        case PWR_DOMAIN_STANDBY:
            // Core systems only — no payload gates needed
            break;

        case PWR_DOMAIN_DEEP_SLEEP:
        default:
            // All gates already LOW from Step 1 — nothing more to do
            break;
    }

    currentDomain = domain;
    Logger_log("PowerManager: domain -> %d (%.1f W)", domain,
               PowerManager_getCurrentPower_W());
}

// ── Getters ──────────────────────────────────────────────────────────────────
PowerDomain_t PowerManager_getCurrentDomain(void) {
    return currentDomain;
}

float PowerManager_getCurrentPower_W(void) {
    switch (currentDomain) {
        case PWR_DOMAIN_DEEP_SLEEP: return PWR_DEEP_SLEEP_W;
        case PWR_DOMAIN_STANDBY:    return PWR_STANDBY_W;
        case PWR_DOMAIN_SENSING:    return PWR_SENSING_W;
        case PWR_DOMAIN_TRANSMIT:   return PWR_TRANSMIT_W;
        default:                    return PWR_DEEP_SLEEP_W;
    }
}
