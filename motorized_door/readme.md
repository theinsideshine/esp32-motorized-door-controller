# ESP32 Motorized Door Controller

Control experimental de una puerta motorizada usando ESP32-S3, motor N20, driver DRV8833 y sensor absoluto AS5048A por SPI.

El objetivo del proyecto es evolucionar de forma conservadora desde un control simple por PWM fijo hacia una arquitectura modular no bloqueante, con máquina de estados y futura incorporación de control PD/PID de posición.

---

## Hardware

- ESP32-S3 Dev Module
- Driver DRV8833
- Motor N20 DC con reductora
- Sensor absoluto AS5048A por SPI
- Fuente externa para motor
- GND común entre lógica y potencia

Conexiones principales usadas en la versión actual:

```text
DRV8833:
STBY  -> GPIO4
AIN2  -> GPIO16
AIN1  -> GPIO17

AS5048A SPI:
CSn   -> GPIO10
MOSI  -> GPIO11
SCK   -> GPIO12
MISO  -> GPIO13
VDD   -> 5V0
GND   -> GND

Final de carrera:
FC_L  -> GPIO14 INPUT_PULLUP
```

---

## Arquitectura actual

La versión actual separa responsabilidades en módulos:

```text
motorized_door.ino
door_config.h / door_config.cpp
door_motion.h / door_motion.cpp
door_motor.h / door_motor.cpp
door_angle_sensor.h / door_angle_sensor.cpp
log.h / log.cpp
```

Responsabilidades:

- `motorized_door.ino`: coordina el producto/dispositivo.
- `door_config`: maneja JSON, NVS y requests pendientes.
- `door_motion`: maneja la FSM de posicionamiento.
- `door_motor`: encapsula DRV8833/PWM.
- `door_angle_sensor`: encapsula lectura del AS5048A.
- `log`: salida configurable de mensajes humanos / plotter / JSON.

El objeto `MagneticSensorAS5048A` y la inicialización SPI se mantienen en el `.ino` por decisión conservadora.

---

## Versión actual validada

```text
v4.0-fsm-hold-ready-no-pid-from-step8F
```

Esta versión nace desde la baseline validada:

```text
v3.1-continuous-silent-measured-config-json-door-motion-motor-sensor-log-step8F-plant-travel-plotter
```

---

## Objetivo de v4.0

La v4.0 prepara la máquina de estados para un futuro modo de mantenimiento de posición, pero todavía no implementa PID ni mantenimiento físico.

Se agrega conceptualmente el estado futuro:

```text
HOLDING
```

Pero en esta versión no se entra todavía a `HOLDING`.

El flujo real validado se mantiene igual:

```text
motion_mode=0:
START -> MOVING -> SETTLING -> IDLE

motion_mode=1:
START -> MOVING -> SETTLING -> IDLE
```

La intención es dejar preparada la arquitectura para una futura versión:

```text
motion_mode=2:
START -> MOVING -> SETTLING -> HOLDING
```

donde `HOLDING` sí tendrá sentido porque el PD/PID podrá leer posición, calcular error, decidir sentido y aplicar PWM continuamente.

---

## Modos de movimiento actuales

### motion_mode = 0

PWM fijo.

Usado como baseline funcional y para identificación de planta.

### motion_mode = 1

Approach no-PID.

Modo comparativo con parámetros:

```text
pwm_start
pwm_slow
slow_zone_deg
start_boost_ms
```

### motion_mode = 2

Reservado para futura implementación PD/PID.

No está implementado en v4.0.

---

## Comandos JSON principales

Información:

```json
{"info":"version"}
{"info":"all-params"}
```

Movimiento:

```json
{"cmd":"go","pos":1}
{"cmd":"go","pos":2}
{"cmd":"go","pos":3}
{"cmd":"stop"}
```

Configuración:

```json
{"motion_mode":0}
{"motion_mode":1}
{"pwm_move":80}
{"log_level":1}
```

Factory reset:

```json
{"cmd":"factory-reset"}
```

---

## Log levels

El nivel de log se guarda en NVS.

Valores usados:

```text
log_level = 0  -> silencioso
log_level = 1  -> mensajes humanos + JSON
log_level = 3  -> modo plotter / salida reducida
```

Si el equipo parece no responder luego de una sesión de plotter, enviar:

```json
{"log_level":1}
```

Luego consultar:

```json
{"info":"version"}
```

---

## Validación v4.0

La v4.0 fue probada con:

```text
ESP32-S3
DRV8833
Motor N20
AS5048A
pwm_move=80
control_period_us=5000
```

### motion_mode=0

Secuencia validada:

```json
{"motion_mode":0}
{"cmd":"go","pos":2}
{"cmd":"go","pos":3}
{"cmd":"go","pos":1}
```

Resultados observados:

```text
POS_2 -> posicion_alcanzada
POS_3 -> posicion_alcanzada
POS_1 -> posicion_alcanzada
stall_count=0/3
```

Tiempos aproximados:

```text
POS_2: 457 ms
POS_3: 472 ms
POS_1: 722 ms
```

También se validó:

```text
- Rechazo de nuevo go durante movimiento.
- Stop durante movimiento.
- Nuevo go después de stop.
- Nuevo go después de llegada.
```

### motion_mode=1

Secuencia validada:

```json
{"motion_mode":1}
{"cmd":"go","pos":2}
{"cmd":"go","pos":3}
{"cmd":"go","pos":1}
```

Resultados observados:

```text
POS_2 -> posicion_alcanzada
POS_3 -> posicion_alcanzada
POS_1 -> posicion_alcanzada
stall_count=0/3
```

Tiempos aproximados:

```text
POS_2: 452 ms
POS_3: 477 ms
POS_1: 722 ms
```

También se validó el rechazo de nuevo `go` durante movimiento:

```text
AUTO ocupado: solo se acepta stop para cancelar.
```

El movimiento original continúa hacia el target original, confirmando que no hay retarget durante `START/MOVING/SETTLING`.

---

## Criterio de aceptación v4.0

La v4.0 queda aceptada porque:

```text
1. Compila.
2. Carga correctamente en ESP32-S3.
3. Responde version/all-params.
4. motion_mode=0 conserva comportamiento validado.
5. motion_mode=1 conserva comportamiento validado.
6. Mientras se mueve, rechaza otro go.
7. Mientras se mueve, acepta stop.
8. Después de llegar, acepta otro go.
9. No activa HOLDING todavía.
10. No introduce PID ni parámetros PID.
```

---

## Nota sobre busy y ack

Actualmente, cuando llega un `go` durante movimiento, la capa JSON puede responder primero:

```json
{"cmd":"go","pos":N,"result":"ack"}
```

Luego la lógica de producto informa:

```text
AUTO ocupado: solo se acepta stop para cancelar.
```

El `ack` significa que el JSON fue recibido, no que el movimiento fue aceptado.

En una versión futura podría mejorarse la semántica para responder algo como:

```json
{"cmd":"go","pos":N,"result":"busy"}
```

No se modifica en v4.0 para conservar la baseline.

---

## Próximo paso previsto

La próxima evolución prevista es:

```text
v4.1-motion-mode-2-pd-position-control
```

Objetivo:

- Agregar `motion_mode=2`.
- Implementar primero PD, con `Ki=0`.
- Controlar posición en lazo cerrado.
- Decidir sentido y PWM en cada muestra.
- Permitir mantenimiento de posición en `HOLDING`.
- Permitir retarget desde el host en modo PID.
- Mantener `motion_mode=0` y `motion_mode=1` sin alterar comportamiento.
