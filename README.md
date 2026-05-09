FireSat-S
Sustainable Wildfire Detection Satellite System

AESH 2026 | Space Systems Engineering

Overview

FireSat-S is a prototype-level autonomous wildfire-detection satellite architecture designed for early wildfire monitoring across wildfire-prone Mediterranean regions.

The project combines:

Orbital mechanics using SGP4 + TLE propagation
MWIR thermal imaging
On-board TinyML inference architecture
Hardware-level power domain management
Distributed FreeRTOS real-time software architecture

FireSat-S focuses on mission-level energy efficiency through orbital-aware subsystem activation rather than continuous full-time sensing.

Instead of keeping all subsystems continuously active, FireSat-S activates sensing, AI processing, and communication hardware only during mission-relevant orbital passes.

This significantly reduces average system power consumption while preserving autonomous wildfire detection capability.

Design Philosophy

FireSat-S was designed around five core engineering principles:

Energy efficiency over continuous sensing
Autonomous subsystem activation
Minimal always-on hardware
Modular RTOS-based subsystem isolation
Scalable constellation-ready architecture

The current implementation prioritizes architectural validation and autonomous power-management logic over continuous global coverage performance.

Project Scope

FireSat-S is currently presented as:

A single-satellite mission architecture
An embedded systems and autonomous power-management concept
A prototype-level TinyML-enabled wildfire detection pipeline
A software and systems-engineering prototype

The current implementation demonstrates:

RTOS task architecture
Queue-based subsystem isolation
Orbital geofencing logic
Autonomous hardware power gating
Communication scheduling
Conceptual AI inference integration

This project is not presented as a fully flight-qualified satellite system.

Several mission components remain prototype-level or conceptual and are intended for future implementation and validation.

Architecture Implementation Status
Subsystem	Status
FreeRTOS Scheduler	Implemented
Queue Communication Architecture	Implemented
Power Gating Logic	Implemented
Logger & Fault Handling	Implemented
Orbital Geofencing Logic	Prototype
SGP4 Integration Logic	Prototype
TinyML Inference Pipeline	Prototype
Real MWIR Hardware Integration	Future Work
Multi-Satellite Coordination	Future Work
Flight Hardware Validation	Future Work
Radiation Testing	Future Work
Key Innovation
Orbital-Aware Autonomous Power Gating

Traditional small satellites often keep payload and communication systems continuously powered.

FireSat-S instead uses orbital-awareness to dynamically control subsystem activation.

Using Two-Line Elements (TLE) and SGP4 orbital propagation, the satellite predicts its position without GPS and activates hardware only when approaching target regions.

Inactive subsystems receive zero electrical power through MOSFET hardware gating.

This reduces:

Average power consumption
Thermal load
Communication overhead
Unnecessary payload operation time
Energy-Aware Mission Philosophy

Most spacecraft subsystems remain OFF during non-target orbital passes.

Normally inactive subsystems:
MWIR thermal payload
TinyML co-processor
S-band transmitter
High-power sensing electronics

These subsystems activate only when:

The satellite approaches a target geofence
A wildfire is detected
A ground station communication window becomes available

This architecture minimizes unnecessary power usage and extends operational efficiency.

Power Consumption Summary
Parameter	Traditional Always-ON	FireSat-S
Average System Power	109 W	22.5 W
Energy per Orbit	178.0 Wh	36.8 Wh
Estimated Power Saving	—	79.3%

These values are based on the current single-satellite architecture and internal mission power analysis.

Mission Configuration
Parameter	Value
Orbit Type	Sun-Synchronous Orbit (SSO)
Altitude	700 km
Inclination	98.18°
Orbit Period	5926 s (~98.8 min)
Eccentricity	~0.001
RAAN	321.5°
Argument of Perigee	90°
Current Architecture	Single Satellite
Future Expansion	Multi-satellite constellation
Target Regions

The mission focuses on wildfire-prone Mediterranean regions.

Region	Latitude Range	Longitude Range
Algeria	18°N → 38°N	3°W → 13.5°E
Greece	34°N → 42°N	19°E → 30°E
Turkey	36°N → 42°N	26°E → 45°E

The payload becomes active only during overpasses above target geofences.

Revisit Time Clarification

Current orbital simulations were performed using a single-satellite configuration.

Preliminary single-satellite STK simulations indicate revisit intervals ranging from approximately 30 minutes in favorable orbital geometry to several hours depending on target region and orbital alignment.

Future constellation expansion using multiple satellites and orbital phase separation can significantly improve revisit frequency and operational coverage.

Why No GPS?

Instead of GPS, FireSat-S uses:

Two-Line Elements (TLE)
SGP4 orbital propagation

Advantages:

Reduced power consumption
Reduced mass and hardware complexity
Deterministic orbital prediction
Improved reliability for long-duration operation
Reduced attack surface
Simplified fault-tolerant operation

The architecture assumes periodic TLE uplinks from ground stations.

Power Domain Management

FireSat-S implements hardware-level MOSFET power gating.

Subsystems are electrically disconnected when inactive rather than placed in standby mode.

Power Modes
Mode	Active Systems	Estimated Power	Usage
Deep Sleep	OBC + ADCS only	~15 W	Outside target zone
Standby	Core systems + comms RX	~30 W	Approaching target
Sensing	MWIR + TinyML	~55 W	Thermal scanning
Transmit	UHF / S-band active	~75 W	Alert transmission
System Software Architecture

FireSat-S uses a distributed FreeRTOS architecture.

Each subsystem operates as an isolated real-time task.

FreeRTOS Scheduler
│
├── OrbitalTask
├── SensingTask
├── CommunicationTask
├── StorageTask
└── PowerManagerTask

All tasks communicate exclusively through FreeRTOS queues.

Mission Execution Flow

Satellite outside target region
↓
Deep Sleep mode
↓
OrbitalTask predicts target approach
↓
PowerManager activates sensing hardware
↓
MWIR frame capture
↓
TinyML inference
↓
Fire detected?
├─ No → discard frame and remain energy-efficient
└─ Yes
↓
UHF emergency alert transmission
↓
Store thermal frame in flash memory
↓
S-band downlink during ground station pass

Payload System
MWIR Thermal Payload
Parameter	Specification
Spectral Range	3.5 – 5.0 µm
Detector Type	HgCdTe FPA
Ground Resolution	100 m GSD
Swath Width	185 km
NETD	< 0.1 K
Cooling	Stirling Cooler @ 77 K
Frame Rate	10 fps
ADC	14-bit
Estimated Active Power	32 W
TinyML Fire Detection System

FireSat-S proposes a lightweight CNN-based TinyML inference architecture for on-board wildfire detection.

The AI layer is currently prototype-level and intended to demonstrate autonomous decision-making capability within the embedded architecture.

The current implementation demonstrates software architecture and inference integration logic rather than a flight-trained wildfire detection model.

Proposed Features
Real-time thermal frame inference
Confidence-based fire classification
Geographic fire localization
Communication bandwidth reduction
Autonomous alert prioritization
Planned Model Characteristics
Parameter	Target Value
Input Size	64 × 64 thermal frame
Inference Hardware	Edge AI Co-processor
Estimated AI Power	8 W
Detection Threshold	0.92 confidence
Minimum Detectable Fire	≥ 1 hectare
Communication System
Link	Hardware	Data Rate	Purpose
S-band	Syrlinks EWC27	10 Mbps	Thermal image downlink
UHF	AstroDev Li-2	9.6 kbps	Emergency wildfire alerts
Fault Tolerance & Reliability
Hardware watchdog timer
Queue-based task isolation
Sensor retry mechanisms
Flash telemetry logging
Fail-safe MOSFET transitions
Autonomous safe-mode fallback
TLE validity verification
Fault-triggered deep-sleep recovery logic
Hardware Targets
Environment	Hardware	Purpose
Prototype	STM32L4 / ESP32-S3	Ground testing
Engineering Model	STM32H7	Embedded validation
Future Flight Target	LEON-based architecture	Radiation-tolerant deployment
ADCS Note

The current project assumes a simplified ADCS layer for mission-level architectural validation.

Current Limitations
Real wildfire dataset training
Flight-qualified AI deployment
Multi-satellite coordination
Real-time ground segment integration
Full radiation testing
Hardware-in-the-loop validation
Orbit correction and station-keeping
Future Improvements
Multi-satellite constellation deployment
Adaptive AI thresholding
SWIR + MWIR multispectral fusion
Autonomous anomaly detection
Real-time telemetry dashboard
Advanced ground station simulator
Inter-satellite communication
Autonomous orbit coordination
Project Structure
FireSat-S/
│
├── src/
│   ├── main.cpp
│   ├── OrbitalTask.cpp
│   ├── SensingTask.cpp
│   ├── CommunicationTask.cpp
│   ├── StorageTask.cpp
│   ├── PowerManager.cpp
│   └── Logger.cpp
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
│   ├── Architecture.png
│   ├── Electrical_Power_System.pdf
│
└── README.md
Technologies

C/C++ • FreeRTOS • STM32H7 • TinyML • SGP4 • MWIR Infrared • S-band • UHF

Team

FireSat-S Team | AESH 2026

Building intelligent and energy-efficient space systems for environmental protection and rapid wildfire response from orbit.
