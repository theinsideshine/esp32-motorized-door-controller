# `esp32-motorized-door-controller`

Firmware y aplicación de escritorio para un prototipo de puerta motorizada basado en ESP32-S3, DRV8833, motor N20, sensor absoluto AS5048A y tira WS2812B.

El repositorio está organizado como monorepo:

```text
firmware / motorized_door
  Control embebido, JSON/NVS, posicionamiento, motor, sensor y FSM LED.

beam_app
  Prototipo de aplicación de escritorio desarrollado con PySide6/Qt.
```

---

## Estado actual

Branch de trabajo:

```text
wip/v5.1a-led-strip-fsm
```

Base firmware validada:

```text
commit 34cba8f  Validate v5.1c LED arrival soft transition
tag    v5.1c-led-arrival-soft-transition
```

Integración posterior del prototipo de aplicación:

```text
commit 489a713  Add beam app prototype
```

Versión propuesta después de actualizar los defaults compilados:

```text
v5.1e-validated-defaults
```

La versión agrega como defaults los valores ya validados con el motor N20 nuevo. No cambia pines, sentidos, protocolo JSON, lógica de movimiento ni FSM LED.

---

## Resultado validado

El prototipo funciona actualmente con:

```text
movimiento real hacia POS_1, POS_2 y POS_3
sensor AS5048A por SPI
control de posición durante MOVING
sin HOLDING activo
FSM LED no bloqueante
transición suave de llegada
simulación runtime de posición para diagnóstico LED
configuración persistente JSON/NVS
Serial Plotter para análisis de planta y control
```

Set operativo validado con el motor nuevo:

```text
motion_mode=2
pid_kp=0.7
pid_ki=0
pid_kd=0
pid_pwm_max=80
pid_pwm_min_effective=70
pid_min_effective_error_deg=2
auto_tolerance_deg=2
control_period_us=5000
```

Con ese set:

```text
sin tironeo del motor
animación LED fluida
reason=posicion_alcanzada
stall_count=0/3
```

---

## Hardware

```text
ESP32-S3 Dev Module
DRV8833
Motor N20 con reductora, VM=6 V
Sensor absoluto AS5048A por SPI
Tira WS2812B
Final de carrera FC_L
Prototipo mecánico impreso en 3D
```

### Pines

```text
DRV8833:
  STBY = GPIO4
  AIN2 = GPIO16
  AIN1 = GPIO17

AS5048A SPI:
  CS   = GPIO10
  MOSI = GPIO11
  SCK  = GPIO12
  MISO = GPIO13

FC_L:
  GPIO14 con INPUT_PULLUP
  NORMAL = LOW
  ACTIVO = HIGH

WS2812B:
  DATA = GPIO6
```

### Posiciones calibradas

```text
POS_1 = 2.29°
POS_2 = 291.23°
POS_3 = 206.06°
```

Sentidos lógicos:

```text
RIGHT / REWIND  -> baja ángulo
LEFT  / FORWARD -> sube ángulo
```

---

## Arquitectura del firmware

```text
motorized_door.ino
  Coordinador del producto.
  Procesa pedidos runtime.
  Inyecta callbacks físicos.
  Coordina DoorMotion, DoorMotor, DoorSensor y LedStrip.

CDoorConfig
  Protocolo JSON por Serial.
  Copia RAM de configuración.
  Persistencia NVS.
  Requests pendientes para el main.

CDoorMotion
  Máquina de estados de posicionamiento.
  START -> MOVING -> SETTLING -> IDLE.
  Llegada, cruce, timeout, stall, cancelación y summary.

CDoorMotor
  Abstracción del DRV8833.
  STBY, AIN1, AIN2, PWM, LEFT, RIGHT y STOP.

CDoorAngleSensor
  Wrapper conservador del AS5048A.
  Posición angular, velocidad y sensor_us.

CLedStrip
  FSM visual no bloqueante para WS2812B.

CLog
  Mensajes humanos, JSON de diagnóstico y salida Arduino Plotter.

CTimer
  Base temporal no bloqueante del firmware.
```

`HOLDING` permanece reservado. El prototipo actual no tiene traba, solenoide, embrague ni mecanismo real de retención que permita validar ese estado de manera concluyente.

---

## Modos de movimiento

```text
motion_mode=0
  PWM fijo.
  Baseline para identificación de planta y diagnóstico.

motion_mode=1
  Perfil de aproximación no-PID.
  Usa boost inicial, zona lenta y PWM lento.

motion_mode=2
  Control de posición configurable P / PI / PD / PID.
  Default actual validado: P puro, Kp=0.7, Ki=0 y Kd=0.
```

El nombre histórico de algunas constantes todavía contiene `PD_POSITION`, pero `motion_mode=2` acepta las tres ganancias y el comportamiento efectivo depende de sus valores.

---

## FSM LED

Comportamiento actual:

```text
LED_STRIP_OFF
  Tira apagada.

LED_STRIP_IDLE
  Azul con respiración.

LED_STRIP_MOVING_FWD
  Verde desplazándose en el sentido visual forward.

LED_STRIP_MOVING_RWD
  Verde desplazándose en el sentido visual rewind.

LED_STRIP_ARRIVED
  Transición suave de llegada antes de volver a IDLE.

LED_STRIP_ALARM
  Rojo ante FC_L activo.
```

Relación actual entre salida de motor y animación:

```text
DoorMotor RIGHT -> LED_STRIP_MOVING_RWD
DoorMotor LEFT  -> LED_STRIP_MOVING_FWD
```

La animación sigue el estado actual del motor. Por eso, durante el diagnóstico del tironeo, una inversión lógica del puente H también se veía como un cambio en el sentido visual de la tira.

---

## `led-sim`: simulación runtime

`led-sim` permite ejecutar el mismo firmware y el mismo camino de movimiento/LED reemplazando únicamente la lectura del AS5048A por una posición virtual.

Ejemplos:

```json
{"cmd":"led-sim","from":1,"to":2,"ms":700}
{"cmd":"led-sim","from":2,"to":3,"ms":850}
{"cmd":"led-sim","from":3,"to":1,"ms":950}
```

Cancelar:

```json
{"cmd":"led-sim-stop"}
```

Para probar sin movimiento físico:

```text
desconectar VM/6 V del motor
mantener ESP32, LED y sensor alimentados
mantener GND común
```

La simulación usa:

```text
DoorMotion normal
DoorMotor normal
FSM LED normal
transición ARRIVED normal
```

La única entrada reemplazada es la posición angular.

---

## Defaults compilados de `v5.1e`

Los siguientes valores quedan definidos en `door_config.h`:

```cpp
#define DOOR_POS_1_DEFAULT_DEG                  2.29f
#define DOOR_POS_2_DEFAULT_DEG                  291.23f
#define DOOR_POS_3_DEFAULT_DEG                  206.06f

#define DOOR_PWM_MOVE_DEFAULT                   80UL
#define DOOR_MOTION_MODE_DEFAULT                DOOR_MOTION_MODE_PD_POSITION

#define DOOR_PID_KP_DEFAULT                     0.70f
#define DOOR_PID_KI_DEFAULT                     0.00f
#define DOOR_PID_KD_DEFAULT                     0.00f
#define DOOR_PID_PWM_MAX_DEFAULT                80UL
#define DOOR_PID_PWM_MIN_EFFECTIVE_DEFAULT      70UL
#define DOOR_PID_MIN_EFFECTIVE_ERROR_DEFAULT    2.0f
#define DOOR_PID_I_ACTIVE_ERROR_DEFAULT         20.0f
#define DOOR_PID_INTEGRAL_LIMIT_DEFAULT         120.0f

#define DOOR_CONTROL_PERIOD_US_DEFAULT          5000UL
#define DOOR_AUTO_TOLERANCE_DEG_DEFAULT         2.00f
#define DOOR_AUTO_CROSS_MARGIN_DEG_DEFAULT      0.20f
#define DOOR_AUTO_MAX_RUN_MS_DEFAULT            10000UL
#define DOOR_AUTO_STALL_CHECK_MS_DEFAULT        250UL
#define DOOR_AUTO_MIN_MOVE_DEG_DEFAULT          0.30f
#define DOOR_AUTO_STALL_MAX_COUNT_DEFAULT       3UL
#define DOOR_AUTO_FINAL_SETTLE_MS_DEFAULT       80UL

#define DOOR_LED_ENABLED_DEFAULT                1UL
#define DOOR_LED_COUNT_DEFAULT                  8UL
#define DOOR_LED_BRIGHTNESS_DEFAULT             25UL
#define DOOR_LED_STEP_MS_DEFAULT                60UL
#define DOOR_LED_BREATH_MS_DEFAULT              25UL
#define DOOR_LED_BLINK_MS_DEFAULT               180UL

#define DOOR_LOG_LEVEL_DEFAULT                  DOOR_LOG_LEVEL_MSG
```

No se cambia `DOOR_CFG_SCHEMA_VERSION`.

Consecuencia:

```text
La configuración NVS existente se conserva.
Un equipo con NVS vacío toma los nuevos defaults.
factory-reset carga y guarda los nuevos defaults.
```

---

## Comandos JSON

### Información

```json
{"info":"version"}
{"info":"all-params"}
```

### Movimiento real

```json
{"cmd":"go","pos":1}
{"cmd":"go","pos":2}
{"cmd":"go","pos":3}
{"cmd":"stop"}
```

### Simulación LED/movimiento lógico

```json
{"cmd":"led-sim","from":1,"to":2,"ms":700}
{"cmd":"led-sim","from":2,"to":3,"ms":850}
{"cmd":"led-sim","from":3,"to":1,"ms":950}
{"cmd":"led-sim-stop"}
```

### Posiciones

```json
{"pos1_deg":2.29}
{"pos2_deg":291.23}
{"pos3_deg":206.06}
```

### Modos y PWM

```json
{"motion_mode":0}
{"motion_mode":1}
{"motion_mode":2}

{"pwm_move":80}
{"pwm_start":80}
{"pwm_slow":60}
{"slow_zone_deg":20}
{"start_boost_ms":120}
```

### Control de posición

```json
{"pid_kp":0.7}
{"pid_ki":0}
{"pid_kd":0}
{"pid_pwm_max":80}
{"pid_pwm_min_effective":70}
{"pid_min_effective_error_deg":2}
{"pid_i_active_error_deg":20}
{"pid_integral_limit":120}
{"control_period_us":5000}
```

### Llegada y seguridad

```json
{"auto_tolerance_deg":2}
{"auto_cross_margin_deg":0.2}
{"auto_max_run_ms":10000}
{"auto_stall_check_ms":250}
{"auto_min_move_deg":0.3}
{"auto_stall_max_count":3}
{"auto_final_settle_ms":80}
```

### LED

```json
{"led_enabled":1}
{"led_enabled":0}
{"led_count":8}
{"led_brightness":25}
{"led_step_ms":60}
{"led_breath_ms":25}
{"led_blink_ms":180}
```

### Log y modo reservado

```json
{"log_level":0}
{"log_level":1}
{"log_level":2}
{"log_level":3}

{"st_mode":0}
{"st_mode":100}
```

`st_mode` está persistido, pero todavía no gobierna una función activa de la máquina superior.

### Restaurar defaults

```json
{"cmd":"factory-reset"}
```

En `v5.1e`, `factory-reset` recupera directamente el set validado del motor nuevo y la FSM LED.

---

## Serial Plotter

Activar salida compatible:

```json
{"log_level":3}
```

Variables actuales:

```text
travel
target
```

Volver a mensajes y respuestas JSON:

```json
{"log_level":1}
```

Para identificación de planta se usa `motion_mode=0`. Para comparar el control se repite el mismo recorrido cambiando solamente el modo o las ganancias.

Ejemplo de baseline:

```json
{"log_level":1}
{"led_enabled":0}
{"motion_mode":0}
{"pwm_move":70}
{"log_level":3}
{"cmd":"go","pos":3}
```

---

## Caso de estudio: motor nuevo y término D

Después de reemplazar el motor N20 apareció un tironeo visible tanto en la mecánica como en la animación LED.

El diagnóstico siguió esta secuencia:

```text
1. led-sim con VM desconectada:
   LED fluido; se descartó la FSM visual como causa autónoma.

2. movimiento real con LED apagado:
   el tironeo continuó; se descartó la carga de la tira.

3. revisión de masa y capacitor bulk de 220 µF:
   sin cambio relevante; la hipótesis de alimentación simple perdió fuerza.

4. motion_mode=0 con PWM fijo:
   movimiento suave; motor, reductora y driver podían trabajar sin tironeo.

5. Arduino Plotter con motion_mode=2 y Kd=0.05:
   se observó retroceso real cerca del objetivo.

6. motion_mode=2 con Kd=0:
   desaparecieron el tironeo mecánico y el salto visual.
```

Interpretación documentada:

```text
La salida de control podía cambiar de signo por acción de D.
El signo seleccionaba el sentido físico del puente H.
Una orden pequeña en sentido contrario era elevada al PWM mínimo efectivo 70.
La FSM LED seguía el estado actual del motor y mostraba la misma inversión.
```

La corrección estructural del término D queda diferida. El set operativo seguro y validado mantiene:

```text
pid_kd=0
```

Documentos asociados:

```text
doc/caso_estudio_I+D_motor_nuevo_PD_v1.pdf
doc/caso_estudio_I+D_motor_nuevo_PD_v1.docx
doc/anexo_relacion_tironeo_motor_animacion_led.pdf
doc/anexo_relacion_tironeo_motor_animacion_led.docx
```

---

## Reglas de trabajo sobre el prototipo

```text
No aplicar cargas laterales aleatorias al eje o cursor.
El juego visible del cursor 3D se acepta para esta etapa sin carga.
No modificar door_motion ni door_motor sin una necesidad clara.
No contaminar el control real del motor para implementar efectos visuales.
Mantener cambios pequeños, reversibles y medibles.
```

Si el conjunto mecánico vuelve a fallar, el próximo paso no será insistir con el mismo montaje: se evaluará un motor y acople más robustos, preparados para la carga real.

---

## Próximos pasos

Orden acordado:

```text
1. Agregar en firmware el estado de bloqueo activado por switch.
2. Representar el bloqueo mediante el modo LED rojo correspondiente.
3. Validar la máquina superior del dispositivo sin alterar DoorMotion.
4. Retomar y completar beam_app con Codex.
5. Integrar el caso de estudio al informe final del proyecto.
```

El informe final presentará el prototipo como un proceso real de I+D, conectando electrónica, firmware, control automático, mecánica, instrumentación, programación, análisis de datos y gestión de riesgo técnico.
