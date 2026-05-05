#  FireSat-S 

### Sustainable Wildfire Detection Satellite System

**AESH 2026 | Space Systems Engineering Project**

---

##  Overview

**FireSat-S** is an intelligent satellite system designed for early wildfire detection using AI while maximizing energy efficiency.

The system integrates:

*  Orbital position estimation without GPS
*  On-board image analysis using TinyML
*  Dynamic power domain management
*  Real-Time Operating System (RTOS)

---

##  Mission Objectives

* Early detection of wildfires
* Minimize power consumption
* Send real-time low-bandwidth alerts
* Store high-resolution data for later transmission

---

##  System Architecture

The system is built using a **Distributed RTOS Architecture**, where functionalities are separated into independent tasks:

* **Orbital Task** → Computes satellite position
* **Sensing Task** → Captures data and runs AI inference
* **Communication Task** → Handles data transmission
* **Storage Task** → Manages onboard memory
* **Power Manager Task** → Controls power domains

All tasks communicate using **RTOS Queues**, ensuring:

* Synchronization
* Fault tolerance
* Scalability

---

##  Power Management Strategy

The system uses **hardware-level power gating** via MOSFETs:

| Mode       | Description                  | Power |
| ---------- | ---------------------------- | ----- |
| Deep Sleep | All systems off              | ~15W  |
| Standby    | Core systems only            | ~30W  |
| Sensing    | Sensor + AI active           | ~55W  |
| Transmit   | Communication systems active | ~75W  |

 Systems are activated only when the satellite is over target مناطق (geofencing)

---

##  Communication Strategy

* **UHF Transmission** → Low-data, real-time alerts
* **S-band Transmission** → High-resolution data downlink over ground stations

---

##  AI Model (TinyML)

* Lightweight on-board model
* Processes MWIR thermal imagery
* Threshold-based wildfire detection

---

##  Orbital Awareness

Instead of using GPS:

* Satellite position is computed using **TLE data**
* Reduces power consumption
* Improves reliability in space environments

---

##  Project Structure

```
FireSat-S/
│
├── src/
│   ├── main.cpp
│   ├── SensingTask.cpp
│   ├── OrbitalTask.cpp
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
│   ├── Architecture.png
│   ├── Power_Analysis.xlsx
│
└── README.md
```

---

##  Technologies Used

* **C/C++ (Embedded Systems)**
* **FreeRTOS**
* **STM32 / ESP32**
* **TinyML**
* **Orbital Mechanics (TLE models)**

---

##  Reliability Features

* Sensor retry mechanisms
* Fault-tolerant communication
* Safe power transitions (fail-safe design)
* Logging system for debugging and telemetry

---

##  Key Innovation

> "An energy-aware autonomous satellite system that activates sensing and AI processing only when the satellite is over target regions, using orbital prediction instead of GPS to drastically reduce power consumption."

---

##  Future Improvements

* Watchdog Timer
* Health Monitoring System
* Ground Station Simulation
* Real-time Telemetry Dashboard

---

## Team Vision

We aim to build an intelligent space system that combines:

* Efficiency
* Intelligence
* Sustainability

To contribute to environmental protection using space technology 

---

## Contact

For collaboration or inquiries:
**FireSat-S Team | AESH 2026**

---

 If you like this project, don’t forget to star the repository!
