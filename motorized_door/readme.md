# ESP32 Motorized Door Controller

Experimental motorized door controller based on an ESP32-S3, DRV8833 motor driver, N20 DC motor, and AS5048A magnetic angle sensor.

## Current version

```cpp
v3.1-continuous-silent-measured-config-json-door-motion-motor-sensor-step6a
```

## Baseline

Previous validated version:

```cpp
v3.1-continuous-silent-measured-config-json-door-motion-motor-step5
```

Step 5 validated that extracting the DRV8833/PWM output to `CDoorMotor` did not change the physical behavior.

## Step 6 objective

This version performs another conservative refactor: the AS5048A SPI angle sensor code was moved out of `motorized_door.ino` into a dedicated sensor module.

No control strategy change was intended.

## Files

```text
motorized_door.ino        Main/device coordinator.
door_config.h/.cpp       JSON host protocol, RAM/NVS configuration and pending requests.
door_motion.h/.cpp       Automatic positioning/motion state machine.
door_motor.h/.cpp        DRV8833/PWM motor output.
door_angle_sensor.h/.cpp AS5048A SPI angle sensor input.
readme.md                Project notes.
```

## Architecture

```text
main / product coordinator
  - setup / loop
  - JSON request coordination
  - DeviceState
  - connects Config, Motion, Motor and Sensor

door_config
  - JSON
  - NVS
  - persisted parameters

door_motion
  - automatic positioning state machine
  - target, direction, arrival, crossing, stall, timeout, cancel, summary

door_motor
  - DRV8833 output
  - STBY, AIN1, AIN2, PWM
  - LEFT, RIGHT, STOP

door_angle_sensor
  - AS5048A SPI initialization
  - sensor.update()
  - angle rad/deg
  - velocity
  - sensor_us timing
```

## Hardware pins

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

## Validated positions

| Position |   Angle |
| -------- | ------: |
| POS_1    |   2.29° |
| POS_2    | 291.23° |
| POS_3    | 206.06° |

## Motor direction convention

```text
LEFT / FORWARD  -> increases measured angle
RIGHT / REWIND  -> decreases measured angle
```

## JSON validation sequence

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

Cancellation:

```json
{"cmd":"go","pos":1}
```

while moving:

```json
{"cmd":"stop"}
```

Expected result:

```text
normal movement -> reason=posicion_alcanzada
host stop       -> reason=cancelado_por_host
stall_count     -> 0/3 under normal conditions with pwm_move=75
control_period_us remains 5000
sensor_us and control_us remain close to Step 5
```

## Notes

PID is still not implemented. The current control strategy remains fixed PWM positioning.

PID should later be introduced inside the motion/positioning layer, not inside host/config or the main product coordinator.


## Step 6a note

Step 6 original movia tambien la construccion/inicializacion del AS5048A y en hardware la lectura quedo clavada en 0.00. Step 6a mantiene SPI.begin() y sensor.init(&SPI) exactamente como Step 5 validado, y solo encapsula lectura/estado en CDoorAngleSensor.
