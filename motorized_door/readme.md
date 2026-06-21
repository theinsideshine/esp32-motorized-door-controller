# ESP32 Motorized Door Controller

Experimental motorized door controller based on an ESP32-S3, DRV8833 motor driver, N20 DC motor, and AS5048A magnetic angle sensor.

The project is being developed incrementally to validate motor control strategies and firmware architecture for a small motorized positioning system.

## Current version

```cpp
v3.1-continuous-silent-measured-config-json-door-motion-motor-step5
```

Baseline before this refactor:

```cpp
v3.1-continuous-silent-measured-config-json-door-motion-step4
```

## Step 5 objective

This version performs a conservative hardware-output refactor.

The DRV8833 motor output code was moved out of `motorized_door.ino` into a dedicated `CDoorMotor` class, while preserving the validated Step 4 behavior.

No control strategy change was intended.

## Files

```text
motorized_door.ino      Main/device coordinator, AS5048A read and host request routing.
door_config.h/.cpp     JSON host protocol, RAM/NVS configuration and pending requests.
door_motion.h/.cpp     Automatic positioning/motion state machine.
door_motor.h/.cpp      DRV8833 output, PWM, STBY and motor state.
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
  - Owns the AS5048A sensor read for now.
  - Reads pending requests from Config.
  - Decides whether to accept go/stop.
  - Calls DoorMotion.start(), DoorMotion.update() and DoorMotion.cancel().
  - Connects DoorMotion with DoorMotor through simple wrapper callbacks.

door_motion
  - Handles automatic movement to target positions.
  - Implements the positioning state machine.
  - Calculates angular error.
  - Selects direction.
  - Detects reached target, crossed target, stall, timeout and host cancellation.
  - Handles final settling and movement summary.

door_motor
  - Encapsulates DRV8833 output.
  - Owns STBY, AIN1 and AIN2.
  - Applies PWM to LEFT or RIGHT.
  - Executes brake/coast stop behavior.
  - Tracks current physical motor output state.
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

```cpp
enum DoorMotorState {
  DOOR_MOTOR_STOPPED,
  DOOR_MOTOR_RIGHT,
  DOOR_MOTOR_LEFT
};
```

The main sketch remains the product/device coordinator.  
The motion module owns automatic positioning logic.  
The motor module owns only the physical DRV8833 output.

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

PID will be introduced later inside the motion/positioning layer, not inside host/config or motor output logic.

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

## Mandatory validation sequence

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

Expected result:

```text
Normal movements -> reason=posicion_alcanzada
Host cancellation -> reason=cancelado_por_host
stall_count=0/3 under the same hardware conditions validated in Step 4
control_period_us=5000 preserved
max_sensor_us and max_control_us comparable to Step 4
```

## Validation rule

This refactor is acceptable only if the hardware behavior remains equivalent to Step 4.

If timing, direction, PWM, stop behavior, stall detection or final summary changes without a clear reason, the refactor must be rejected.

