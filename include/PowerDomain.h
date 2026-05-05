/**
 * @file    PowerDomain.h
 * @brief   Hardware power domain definitions — FireSat-S
 * @note    All power values verified against docs/Power_Analysis.xlsx
 *          No Gating: 178.0 Wh/orbit | With Gating: 36.8 Wh/orbit (79.3% saving)
 */

#ifndef POWER_DOMAIN_H
#define POWER_DOMAIN_H

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"

// ── Power consumption per domain (W) — source: Power_Analysis.xlsx ──────────
static constexpr float PWR_DEEP_SLEEP_W  = 15.0f;   // OBC + ADCS only
static constexpr float PWR_STANDBY_W     = 30.0f;   // + comms RX
static constexpr float PWR_SENSING_W     = 55.0f;   // + MWIR + AI
static constexpr float PWR_TRANSMIT_W    = 75.0f;   // + S-band or UHF
static constexpr float PWR_PEAK_W        = 109.0f;  // all systems peak

// ── Energy budget (Wh/orbit) ─────────────────────────────────────────────────
static constexpr float ENERGY_NO_GATING_Wh  = 178.0f;
static constexpr float ENERGY_GATED_Wh      = 36.8f;
static constexpr float POWER_SAVING_PCT      = 79.3f;

// ── MOSFET hard gate pins (STM32H7, GPIOB) ──────────────────────────────────
// Prototype mapping (STM32L4/ESP32): see docs/hardware_mapping.md
#define GATE_PORT         GPIOB
#define PIN_GATE_MWIR     GPIO_PIN_12   // 32 W — Stirling cooler included
#define PIN_GATE_AI       GPIO_PIN_14   //  8 W — edge AI co-processor
#define PIN_GATE_SBAND    GPIO_PIN_13   // 15 W — S-band transmitter
#define PIN_GATE_UHF      GPIO_PIN_15   //  5 W — UHF emergency TX
#define PIN_GATE_ALL     (PIN_GATE_MWIR | PIN_GATE_AI | PIN_GATE_SBAND | PIN_GATE_UHF)

// ── MWIR cooler stabilization time ──────────────────────────────────────────
#define MWIR_COOLER_STABILIZE_MS   500U   // Stirling cooler warm-up (ms)

// ── Power domain states ──────────────────────────────────────────────────────
typedef enum : uint8_t {
    PWR_DOMAIN_DEEP_SLEEP = 0,   // default / safe state
    PWR_DOMAIN_STANDBY    = 1,
    PWR_DOMAIN_SENSING    = 2,
    PWR_DOMAIN_TRANSMIT   = 3
} PowerDomain_t;

// ── Public API ───────────────────────────────────────────────────────────────
void PowerManager_init(void);
void PowerManager_setDomain(PowerDomain_t domain);
PowerDomain_t PowerManager_getCurrentDomain(void);
float PowerManager_getCurrentPower_W(void);

#endif /* POWER_DOMAIN_H */
