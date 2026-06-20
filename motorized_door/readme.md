# ESP32 Motorized Door Controller

Experimental motorized door controller based on an ESP32-S3, DRV8833 motor driver, N20 DC motor, and AS5048A magnetic angle sensor.

The project is being developed incrementally to validate motor control strategies and firmware architecture for a small motorized positioning system.

## Current version

```cpp
v3.1-continuous-silent-measured-config-json-door-motion-step4
```

Baseline before this refactor:

```cpp
v3.1-continuous-silent-measured-config-json-position-fsm-step3
```

## Step 4 objective

This version performs a conservative architecture refactor.

The positioning/motion state machine was moved out of `motorized_door.ino` into a dedicated motion module, while preserving the validated physical behavior from Step 3.

No control strategy change was intended.

## Files

```text
motorized_door.ino      Main/device coordinator and validated hardware functions.
door_config.h/.cpp     JSON host protocol, RAM/NVS configuration and pending requests.
door_motion.h/.cpp     Automatic positioning/motion state machine.
readme.md              Project notes.
```

## Hardware

Validated hardware:

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

```text
LEFT / FORWARD  -> increases measured angle
RIGHT / REWIND  -> decreases measured angle
```

## Calibrated positions

| Position |   Angle |
| -------- | ------: |
| POS_1    |   2.29° |
| POS_2    | 291.23° |
| POS_3    | 206.06° |

## Architecture

Current split:

```text
door_config
  - Receives JSON commands from Serial.
  - Stores and loads configuration using NVS.
  - Exposes pending requests to the main program.
  - Does not control the motor.

motorized_door.ino
  - Coordinates the device.
  - Owns the validated hardware functions.
  - Reads pending requests from Config.
  - Decides whether to accept go/stop.
  - Calls DoorMotion.start(), DoorMotion.update() and DoorMotion.cancel().

door_motion
  - Handles automatic movement to target positions.
  - Implements the positioning state machine.
  - Calculates angular error.
  - Selects direction.
  - Detects reached target, crossed target, stall, timeout and host cancellation.
  - Handles final settling and movement summary.
```

State split:

```cpp
enum DeviceState {
  DEV_IDLE,
  DEV_MANUAL_MOVING,
  DEV_POSITIONING
};
```

```cpp
enum DoorMotionState {
  DOOR_MOTION_IDLE,
  DOOR_MOTION_START,
  DOOR_MOTION_MOVING,
  DOOR_MOTION_SETTLING
};
```

The main sketch remains the product/device coordinator.
The motion module owns the automatic positioning logic.

## Current control strategy

This version does not include PID yet.

The current positioning strategy is:

```text
1. Read current angle.
2. Calculate angular error to target.
3. Select LEFT or RIGHT direction.
4. Move with fixed configured PWM.
5. Stop when target is reached or crossed.
6. Wait final settle time.
7. Print summary.
```

PID will be introduced later inside the motion/positioning layer, not inside host/config logic.

## JSON commands

Read firmware version:

```json
{"info":"version"}
```

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

## Hardware validation

Step 4 was validated in hardware with:

```json
{"info":"version"}
```

```json
{"info":"all-params"}
```

```json
{"pwm_move":75}
```

```json
{"cmd":"go","pos":2}
```

```json
{"cmd":"go","pos":3}
```

```json
{"cmd":"go","pos":1}
```

```json
{"cmd":"go","pos":3}
```

Cancellation test:

```json
{"cmd":"go","pos":1}
```

while moving:

```json
{"cmd":"stop"}
```

Observed result:

```text
BOOT OK - v3.1-continuous-silent-measured-config-json-door-motion-step4

{"info":"version"}      -> OK
{"info":"all-params"}   -> OK, includes app_version
{"pwm_move":75}         -> OK

POS_2 -> reason=posicion_alcanzada, stall_count=0/3
POS_3 -> reason=posicion_alcanzada, stall_count=0/3
POS_1 -> reason=posicion_alcanzada, stall_count=0/3
POS_3 -> reason=posicion_alcanzada, stall_count=0/3

Host stop while moving -> reason=cancelado_por_host, stall_count=0/3
```

Timing remained stable:

```text
control_period_us = 5000
min_sample_dt_us  = 5000
max_sample_dt_us  ≈ 5003 / 5005
max_sensor_us     ≈ 32 / 39
max_control_us    ≈ 40 / 59
```

## Validation conclusion

The Step 4 refactor preserves the validated Step 3 physical behavior.

The automatic positioning logic is now separated into `door_motion`, while the main sketch remains responsible for product coordination and validated hardware access.

This version is a stable baseline before introducing PID or speed profiling.

