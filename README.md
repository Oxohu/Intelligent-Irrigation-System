# Intelligent Irrigation System (ESP32 + Web Server)

A WiFi-enabled irrigation system built on ESP32 that combines real-time soil and water sensing, remote web-based control, automatic irrigation logic, and an ML-inspired decision layer for adaptive watering.

Designed for small to medium-scale farms, the system prioritises water efficiency, transparency, and human-in-the-loop control.

---

## What This System Does

The system continuously monitors soil moisture and water level sensors, controls a pump through a relay, and exposes a responsive web dashboard hosted directly on the ESP32.

Users can:
- Manually turn irrigation on or off
- Switch to rule-based automatic mode
- Enable ML auto mode for adaptive watering decisions
- View live sensor readings and historical trends
- Download sensor data as CSV for analysis

All interaction happens over a local web interface served by the microcontroller.

---

## Core Features

- ESP32 WiFi web server with REST-style endpoints
- Real-time soil moisture and water level monitoring
- Manual override, automatic mode, and ML auto mode
- Dynamic pump timing with safety cutoffs
- Built-in data logging and history buffer
- Live charts rendered directly in the browser
- CSV export for offline analysis
- Visual and audible alerts for low water conditions

---

## ML Auto Mode (Logic-Based)

The ML auto mode simulates intelligent decision-making by considering:
- Current soil moisture
- Available water level
- Time since last watering
- Estimated soil drying rate

Based on these factors, the system decides whether watering is necessary and adjusts pump duration accordingly. This provides adaptive behavior without relying on external cloud services.

---

## System Architecture

- ESP32 microcontroller
- Soil moisture sensor (analog)
- Water level sensor (analog)
- Relay-controlled water pump
- Buzzer and LED for local alerts
- Embedded HTML, CSS, and JavaScript dashboard
- ArduinoJson for structured data exchange

The entire stack runs locally on the device.

---

## Why This Project Matters

This project demonstrates how embedded systems, networking, and data-driven logic can be combined to solve real agricultural problems.

It reflects a practical approach to smart farming in low-resource environments, where reliability, explainability, and offline capability matter more than cloud dependency.


## Location Context

Tested with farmland parameters based on Niger State, Nigeria, including soil type, crop selection, and water availability constraints.


Built by Nafisat  
Embedded systems, data-driven automation, and human-centered engineering.
