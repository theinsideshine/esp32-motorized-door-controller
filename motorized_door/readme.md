# git commit -m "feat: add JSON config and position FSM baseline"
# ESP32 Motorized Door Controller


Experimental motorized door controller based on an ESP32-S3, a DRV8833 motor driver, an N20 DC motor, and an AS5048A magnetic angle sensor.

The project is being developed incrementally to compare different control strategies and firmware architectures for a small motorized positioning system.

## Current status

Current validated firmware baseline:

```cpp
v3.1-continuous-silent-measured-config-json-position-fsm-step3
```

This version validates:

* JSON-based host communication.
* Runtime configuration through JSON commands.
* Persistent configuration using ESP32 NVS.
* Positioning commands through JSON.
* Stop/cancel command through JSON.
* A formal positioning finite state machine.
* Same physical movement behavior as the previously validated continuous control version.
* Stable operation with `pwm_move = 75` in the latest test sequence.
* Configuration persistence after power loss.

## Hardware

Main components:

* ESP32-S3 development board.
* DRV8833 dual H-bridge motor driver.
* N20 DC gear motor.
* AS5048A magnetic angle sensor over SPI.
* Mechanical end-stop / limit switch.

Validated pinout:

| Function          | ESP32-S3 GPIO |
| ----------------- | ------------: |
| DRV8833 AIN1      |        GPIO17 |
| DRV8833 AIN2      |        GPIO16 |
| DRV8833 STBY      |         GPIO4 |
| AS5048A CSn       |        GPIO10 |
| AS5048A MOSI      |        GPIO11 |
| AS5048A SCK       |        GPIO12 |
| AS5048A MISO      |        GPIO13 |
| FC_L limit switch |        GPIO14 |

Motor direction convention:

* `LEFT / FORWARD` increases the measured angle.
* `RIGHT / REWIND` decreases the measured angle.

## Validated positions

Current calibrated target positions:

| Position |   Angle |
| -------- | ------: |
| POS_1    |   2.29° |
| POS_2    | 291.23° |
| POS_3    | 206.06° |

## Firmware architecture

The project is intentionally evolving step by step.

Current architecture:

```text
Config
  - Receives JSON commands from Serial.
  - Stores configuration in RAM and NVS.
  - Exposes pending requests to the main program.
  - Does not move the motor.

Main / Device layer
  - Reads pending requests from Config.
  - Starts or cancels positioning.
  - Coordinates the current device state.

Position FSM
  - Handles automatic movement to POS_1, POS_2 or POS_3.
  - Calculates angular error.
  - Selects motor direction.
  - Detects target reached, stall, timeout, host cancel, and settling.
  - Produces the movement summary.

Motor output
  - Applies LEFT, RIGHT or STOP to the DRV8833.
```

Current finite state machines:

```cpp
enum DeviceState {
  DEV_IDLE,
  DEV_MANUAL_MOVING,
  DEV_POSITIONING
};
```

```cpp
enum PositionState {
  POSITION_IDLE,
  POSITION_START,
  POSITION_MOVING,
  POSITION_SETTLING
};
```

## JSON command examples

Read all parameters:

```json
{"info":"all-params"}
```

Set movement PWM:

```json
{"pwm_move":75}
```

Move to a position:

```json
{"cmd":"go","pos":1}
```

```json
{"cmd":"go","pos":2}
```

```json
{"cmd":"go","pos":3}
```

Stop current movement:

```json
{"cmd":"stop"}
```

Factory reset configuration:

```json
{"cmd":"factory-reset"}
```

## Notes about the current control strategy

The current control is not PID yet.

The system uses a simple continuous movement strategy:

* Select direction from angular error.
* Move with configured PWM.
* Stop when the target is reached or crossed.
* Wait a final settling time.
* Report the final position and error.

The latest tests show that `pwm_move = 70` can be marginal for some starts, while `pwm_move = 75` worked reliably in the validated test sequence.

The measured final error still shows mechanical overshoot due to inertia. This is expected and will be useful later to justify speed profiling and PID control.

## Development roadmap

Planned evolution:

1. Keep the validated JSON/NVS configuration layer.
2. Keep refining the positioning finite state machine.
3. Extract responsibilities into classes only after the behavior is stable.
4. Add motor and sensor abstractions.
5. Add speed profiling.
6. Introduce PID control.
7. Later, compare the Arduino finite-state-machine approach with an ESP-IDF event-driven architecture.

## Repository purpose

This repository is both a working firmware project and a technical learning path.

The goal is not only to move a small motorized door, but also to document why the firmware evolves from a simple blocking sketch into a non-blocking architecture based on explicit states, configuration, measured behavior, and eventually PID control.
