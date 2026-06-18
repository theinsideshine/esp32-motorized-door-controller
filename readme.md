This repository documents the evolution of an ESP32-based motorized door position controller.

The project starts with a monolithic Arduino sketch used to validate the motor driver, position sensor, and basic motion logic. From there, it evolves toward a modular firmware design, PID-based position control, non-blocking state machines, and eventually an ESP-IDF event-driven architecture.

The main goal is not only to control a motorized mechanism, but also to compare firmware architecture approaches:
- blocking vs non-blocking control
- monolithic sketch vs modular design
- open-loop/profile-based motion vs PID control
- Arduino state machines vs ESP-IDF event-driven systems