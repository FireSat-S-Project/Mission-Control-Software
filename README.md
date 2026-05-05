# FireSat-S
### Sustainable Wildfire Detection Satellite System
**AESH 2026 | Space Systems Engineering**

---

## Overview

FireSat-S is an autonomous satellite system for early wildfire detection over Algeria and Tunisia, combining orbital mechanics, thermal infrared sensing, and on-board AI — while minimizing power consumption through hardware-level power domain management.

**Key Innovation:**
> An energy-aware autonomous satellite system that activates sensing and AI processing only when the satellite is over target regions, using orbital position prediction (TLE/SGP4) instead of GPS — reducing average system power from **109 W to 22.5 W** (a saving of **79.3%**).

---

## Mission Parameters

| Parameter | Value |
|-----------|-------|
| Orbit type | Sun-Synchronous (SSO) |
| Altitude | 700 km |
| Inclination | 98.2° |
| Orbit period | 98.8 min |
| Satellites | 2 (180° phase separation) |
| Target region | Algeria + Tunisia (18°–38°N, 3°W–13.5°E) |
| Active coverage fraction | 8% of each orbit |
| Revisit time | < 25 min (dual satellite) |
| Latency requirement | < 30 min  achieved in < 15 min |

---

## Power Domain Management

The system implements **hardware-level MOSFET power gating** — components receive zero power (not standby) when inactive. Domains are activated only when the satellite is confirmed over the target geofence.

### Power Modes

| Mode | Active Systems | Power | When |
|------|---------------|-------|------|
| Deep Sleep | OBC + ADCS only | ~15 W | Outside target zone (92% of orbit) |
| Standby | Core + comms RX | ~30 W | Approaching zone |
| Sensing | + MWIR + AI Unit | ~55 W | Over target, scanning |
| Transmit | + S-band or UHF | ~75 W | Alert or downlink |

### Power Budget Results (from `docs/Power_Analysis.xlsx`)

| Scenario | Avg Power | Energy/Orbit | Saving |
|----------|-----------|--------------|--------|
| No power gating (always on) | 109.0 W | 178.0 Wh | — |
| With geofenced power gating | 22.5 W | 36.8 Wh | **79.3%** |

**MWIR sensor average power:** 2.56 W (vs. 32 W continuous) — 92% payload saving.

> Verified by `docs/Power_Analysis.xlsx` — all figures derived from Excel formulas, not hardcoded.

---

## System Architecture

The firmware uses a **Distributed FreeRTOS Architecture** — each subsystem runs as an independent task with defined priority and stack size. Tasks communicate through RTOS Queues, ensuring deterministic timing and fault isolation.

```
┌─────────────────────────────────────────────────────┐
│                   FreeRTOS Scheduler                │
├──────────────┬──────────────┬───────────────────────┤
│ OrbitalTask  │ SensingTask  │  CommunicationTask    │
│ (HIGH prio)  │ (HIGH prio)  │  (MEDIUM prio)        │
│              │              │                       │
│ SGP4 TLE     │ MWIR capture │ UHF alert TX          │
│ propagation  │ TinyML infer │ S-band data downlink  │
│ Geofence     │ Fire detect  │ Ground station sync   │
│ check        │ Flash store  │                       │
├──────────────┴──────────────┴───────────────────────┤
│           PowerManager  (HIGHEST prio)              │
│   Hard MOSFET gating — fail-safe on all transitions │
├─────────────────────────────────────────────────────┤
│           StorageTask  (LOW prio)                   │
│   Flash write queue, downlink buffer management     │
└─────────────────────────────────────────────────────┘
         All tasks communicate via RTOS Queues
```

### Why no GPS?
Satellite position is computed on-board using **SGP4 orbital propagation** from Two-Line Element (TLE) sets uplinked from ground. This eliminates GPS hardware (saves ~2 W, ~0.4 kg) and improves reliability — GPS signals are unreliable at 700 km altitude and not needed when orbital mechanics are deterministic.

---

## Sensors & Components

### Payload

| Component | Specification |
|-----------|--------------|
| **MWIR Imager** | HgCdTe FPA, 3.5–5.0 µm, 100 m GSD, 185 km swath |
| Aperture | 0.263 m diameter |
| Sensitivity (NETD) | < 0.1 K |
| Detector cooling | Stirling cooler @ 77 K |
| Frame rate | 10 fps, 14-bit ADC |
| Mass | 28 kg |
| Power (active) | 32 W |

### ADCS

| Component | Model | Specification |
|-----------|-------|--------------|
| Star Tracker | Sinclair ST-16RT2 | < 2 arcsec pointing accuracy |
| Reaction Wheels | Bradford ECAPS ×4 | Pyramid config, 0.12 N·m·s each |
| IMU | Sensonor STIM300 | < 0.5°/hr gyro bias |
| Sun Sensors | CSS ×6 | Full 4π coverage, 0.5° accuracy |
| Magnetometer | LEMI-011 | ±200 µT, detumbling + ADCS support |

### Power System

| Component | Specification |
|-----------|--------------|
| Solar arrays | 2× deployable, 3J GaAs, 2.2 m², 220 W BOL |
| Battery | Li-Ion 40 Ah @ 28 V = 1,120 Wh, DOD ≤ 30% |
| Available margin | 136 W avg available vs. 109 W peak — **20% margin** |

### Communications

| Link | Hardware | Rate | Use |
|------|----------|------|-----|
| S-band | Syrlinks EWC27, 2200–2290 MHz | 10 Mbps | Image downlink |
| UHF | AstroDev Li-2, 400–450 MHz | 9.6 kbps | Fire alerts (< 200 bytes) |

---

## AI Model (TinyML)

- Lightweight CNN deployed on edge AI co-processor (8 W active)
- Input: MWIR thermal frame at 100 m GSD
- Output: fire detected (bool) + confidence score + bounding coordinates
- Detection threshold: confidence > 0.92 (tuned to minimize false positives)
- Detection capability: fires ≥ 1 hectare
- Data reduction: only alerts transmitted immediately (UHF), full images queued for S-band — **reduces comms power by ~85%** (fires detected in ~15% of passes over target)

---

## Hardware Targets

| Environment | Hardware | Justification |
|-------------|----------|---------------|
| Ground prototype / testing | STM32L4 / ESP32-S3 | Low cost, rapid development |
| Engineering model | STM32H7 (radiation-tolerant) | Enhanced reliability |
| Flight model | SPARC Leon3 (radiation-hardened) | Survives 20 krad TID over 5-year mission |

> Commercial MCUs (ESP32, STM32L4) are used for ground development only. Space hardware must be radiation-qualified. See `docs/` for radiation tolerance analysis.

---

## Project Structure

```
FireSat-S/
│
├── src/
│   ├── main.cpp               ← FreeRTOS task initialization + scheduler start
│   ├── SensingTask.cpp        ← MWIR capture + TinyML inference loop
│   ├── OrbitalTask.cpp        ← SGP4 TLE propagation + geofence logic
│   ├── CommunicationTask.cpp  ← UHF alert TX + S-band downlink queue
│   ├── StorageTask.cpp        ← Flash write queue + downlink buffer
│   └── PowerManager.cpp       ← MOSFET hard gating + safe state transitions
│
├── include/
│   ├── OrbitalMechanics.h     ← SGP4 API + GeoPosition_t struct
│   ├── TinyML_FireModel.h     ← Inference API + FireAlert_t struct
│   ├── PowerDomain.h          ← PowerDomain_t enum + setPowerDomain()
│   ├── Logger.h               ← Telemetry logging + fault reporting
│   └── Queues.h               ← RTOS queue handles (shared across tasks)
│
├── docs/
│   ├── Power_Analysis.xlsx    ← Full power budget + energy-per-orbit analysis
│   └── Architecture.png       ← System block diagram
│
└── README.md
```

---

## Reliability Features

- **Watchdog Timer** — hardware reset if any task stalls > 5 s
- **Fail-safe power transitions** — all MOSFET gates default LOW on reset
- **Sensor retry mechanism** — 3 retries with exponential backoff before fault flag
- **Fault-tolerant communication** — UHF beacon continues independently of S-band
- **Telemetry logging** — all task states and power transitions logged to flash
- **Radiation mitigation** — TMR (Triple Modular Redundancy) on critical OBC registers

---

## Key Results Summary

| Metric | Without FireSat-S | With FireSat-S |
|--------|------------------|----------------|
| Wildfire detection latency | Hours (ground patrol) | < 15 minutes |
| Average system power | 109 W (always-on) | 22.5 W (gated) |
| Energy per orbit | 178.0 Wh | 36.8 Wh |
| Power saving | — | **79.3%** |
| Daily revisits per location | 0 (no satellite) | 4× (dual sat) |
| Alert transmission time | — | < 2 min (UHF) |

---

## Future Improvements

- [ ] Watchdog Timer integration (firmware)
- [ ] Health Monitoring System with autonomous anomaly detection
- [ ] Ground Station Simulation (Python)
- [ ] Real-time Telemetry Dashboard (web)
- [ ] Multi-spectral fusion (SWIR + MWIR) for smoke penetration

---

## Technologies

`C/C++` `FreeRTOS` `STM32H7` `TinyML` `SGP4 Orbital Mechanics` `MWIR Infrared` `S-band` `UHF`

---

## Team

**FireSat-S Team | AESH 2026**
Building intelligent space systems that combine efficiency, intelligence, and sustainability — for environmental protection from orbit.

---

*If this project helped you, please star the repository.*
