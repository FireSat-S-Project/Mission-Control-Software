# FireSat-S

### Sustainable Wildfire Detection Satellite System

**AESH 2026 | Space Systems Engineering**

---

# Overview

**FireSat-S** is an autonomous satellite system for early wildfire detection across **Algeria, Greece, and Turkey**, combining:

* Orbital mechanics (SGP4 + TLE propagation)
* MWIR thermal imaging
* On-board TinyML inference
* Hardware-level power domain management

The system intelligently activates sensing and AI processing **only when the satellite is over target regions**, dramatically reducing average system power consumption.

---

# Key Innovation

FireSat-S replaces continuous always-on operation with a **geofenced autonomous power architecture**.

Using **TLE/SGP4 orbital propagation instead of GPS**, the satellite predicts its own orbital position and activates subsystems only during mission-relevant overpasses.

### Result:

| Parameter            | Traditional Always-ON | FireSat-S |
| -------------------- | --------------------- | --------- |
| Average System Power | 109 W                 | 22.5 W    |
| Energy per Orbit     | 178.0 Wh              | 36.8 Wh   |
| Power Saving         | —                     | **79.3%** |

---

# Mission Parameters

| Parameter                | Value                        |
| ------------------------ | ---------------------------- |
| Orbit Type               | Sun-Synchronous Orbit (SSO)  |
| Altitude                 | 700 km                       |
| Inclination              | 98.18°                       |
| Orbit Period             | 5926.38 s (~98.8 min)        |
| Eccentricity             | ~0.001 (Near Circular Orbit) |
| RAAN                     | 321.5°                       |
| Argument of Perigee      | 90°                          |
| Satellites               | 2 (180° phase separation)    |
| Target Regions           | Algeria • Greece • Turkey    |
| Active Coverage Fraction | ~8% of orbit                 |
| Revisit Time             | < 25 min                     |
| Alert Latency            | < 15 min                     |

---

# Target Regions

The mission focuses on wildfire-prone Mediterranean regions:

| Country | Latitude Range | Longitude Range |
| ------- | -------------- | --------------- |
| Algeria | 18°N → 38°N    | 3°W → 13.5°E    |
| Greece  | 34°N → 42°N    | 19°E → 30°E     |
| Turkey  | 36°N → 42°N    | 26°E → 45°E     |

The satellite only activates thermal sensing and AI inference when passing over these regions.

---

# Power Domain Management

FireSat-S implements **hardware-level MOSFET power gating**.

Inactive subsystems receive **zero electrical power** (not standby mode), significantly reducing energy consumption and thermal load.

## Power Modes

| Mode       | Active Systems          | Power | Usage               |
| ---------- | ----------------------- | ----- | ------------------- |
| Deep Sleep | OBC + ADCS only         | ~15 W | Outside target zone |
| Standby    | Core systems + comms RX | ~30 W | Approaching target  |
| Sensing    | MWIR + TinyML           | ~55 W | Wildfire scanning   |
| Transmit   | S-band / UHF active     | ~75 W | Alert transmission  |

---

# System Architecture

The firmware uses a **Distributed FreeRTOS Architecture** where each subsystem operates as an independent real-time task.

```text
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
│   Hardware MOSFET gating + safe transitions         │
├─────────────────────────────────────────────────────┤
│             StorageTask  (LOW prio)                 │
│     Flash buffer + downlink queue management        │
└─────────────────────────────────────────────────────┘
```

All tasks communicate exclusively through **FreeRTOS Queues**.

---

# Why No GPS?

Instead of GPS, FireSat-S uses:

* **Two-Line Elements (TLE)**
* **SGP4 orbital propagation**

Advantages:

* Saves ~2 W power
* Saves ~0.4 kg mass
* Reduces hardware complexity
* Higher reliability in LEO operations
* Deterministic orbital prediction

Satellite position error remains below **1 km** when TLE age is less than 72 hours.

---

# Payload System

## MWIR Thermal Payload

| Parameter         | Specification          |
| ----------------- | ---------------------- |
| Spectral Range    | 3.5 – 5.0 µm           |
| Detector Type     | HgCdTe FPA             |
| Ground Resolution | 100 m GSD              |
| Swath Width       | 185 km                 |
| Aperture Diameter | 0.263 m                |
| NETD              | < 0.1 K                |
| Cooling           | Stirling Cooler @ 77 K |
| Frame Rate        | 10 fps                 |
| ADC               | 14-bit                 |
| Active Power      | 32 W                   |
| Mass              | 28 kg                  |

---

# Edge AI System (TinyML)

A lightweight CNN model performs on-board wildfire detection.

## Features

* Real-time thermal image inference
* Confidence-based fire classification
* Bounding coordinate estimation
* False-positive minimization
* Communication bandwidth reduction

## Model Characteristics

| Parameter               | Value                 |
| ----------------------- | --------------------- |
| Input Size              | 64 × 64 thermal frame |
| Inference Hardware      | Edge AI Co-processor  |
| Active Power            | 8 W                   |
| Detection Threshold     | 0.92 confidence       |
| Minimum Detectable Fire | ≥ 1 hectare           |
| Detection Accuracy      | > 95%                 |

---

# ADCS (Attitude Determination & Control)

| Component       | Specification                 |
| --------------- | ----------------------------- |
| Star Tracker    | < 2 arcsec accuracy           |
| Reaction Wheels | 4-wheel pyramid configuration |
| IMU             | < 0.5°/hr gyro bias           |
| Sun Sensors     | Full 4π coverage              |
| Magnetometer    | ±200 µT                       |

---

# Communication System

| Link   | Hardware       | Data Rate | Purpose                   |
| ------ | -------------- | --------- | ------------------------- |
| S-band | Syrlinks EWC27 | 10 Mbps   | Full image downlink       |
| UHF    | AstroDev Li-2  | 9.6 kbps  | Emergency wildfire alerts |

## Communication Strategy

### UHF Path

* Immediate wildfire alerts
* < 200 bytes packets
* < 2 min delivery latency

### S-band Path

* High-resolution thermal frames
* Downlink during ground station visibility windows

---

# Hardware Targets

| Environment       | Hardware           | Purpose                       |
| ----------------- | ------------------ | ----------------------------- |
| Prototype         | STM32L4 / ESP32-S3 | Ground testing                |
| Engineering Model | STM32H7            | Reliability validation        |
| Flight Model      | SPARC Leon3        | Radiation-hardened deployment |

---

# Reliability Features

* Hardware Watchdog Timer
* Fail-safe MOSFET transitions
* Queue-based task isolation
* Sensor retry mechanisms
* Flash telemetry logging
* Radiation mitigation (TMR)
* Autonomous safe-mode fallback
* Fault-tolerant communication paths

---

# Project Structure

```text
FireSat-S/
│
├── src/
│   ├── main.cpp
│   ├── OrbitalTask.cpp
│   ├── SensingTask.cpp
│   ├── CommunicationTask.cpp
│   ├── StorageTask.cpp
│   └── PowerManager.cpp
│
├── include/
│   ├── OrbitalMechanics.h
│   ├── TinyML_FireModel.h
│   ├── PowerDomain.h
│   ├── Logger.h
│   └── Queues.h
│
├── docs/
│   ├── Power_Analysis.xlsx
│   └── Architecture.png
│
└── README.md
```

---

# Key Results Summary

| Metric             | Traditional Systems | FireSat-S  |
| ------------------ | ------------------- | ---------- |
| Detection Latency  | Hours               | < 15 min   |
| Average Power      | 109 W               | 22.5 W     |
| Energy per Orbit   | 178.0 Wh            | 36.8 Wh    |
| Power Saving       | —                   | 79.3%      |
| Alert Transmission | Delayed             | < 2 min    |
| Daily Revisits     | Limited             | 4× per day |

---

# Future Improvements

* Health Monitoring AI
* Autonomous anomaly detection
* Multi-spectral fusion (SWIR + MWIR)
* Real-time telemetry dashboard
* Ground station simulator
* Adaptive AI thresholding
* Autonomous orbit correction

---

# Technologies

`C/C++` • `FreeRTOS` • `STM32H7` • `TinyML` • `SGP4` • `MWIR Infrared` • `S-band` • `UHF`

---

# Team

## FireSat-S Team | AESH 2026

Building intelligent and energy-efficient space systems for environmental protection and rapid wildfire response from orbit.

---

If this project helped you, please  star the repository.
