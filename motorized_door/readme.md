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

Baseline estable de partida:

```text
commit 7883b25  Set validated defaults for new motor
tag    v5.1e-validated-defaults
```

Versión de esta evolución:

```text
v5.2b-danger-button-fsm
```

La versión conserva la FSM superior de demo y agrega un estado `DANGER` por pulsador. El evento se acepta solamente en `DEV_READY`, con la puerta centrada en POS_2; activa rojo intermitente durante un tiempo configurable y vuelve a `READY`. No cambia pines, PID, `motion_mode`, lógica fina de movimiento ni `beam_app`.

---

## Resultado validado

El prototipo funciona actualmente con:

```text
centrado automático al arrancar en modo normal
ciclo fwd: POS_2 -> POS_1 -> espera -> POS_2
ciclo rew: POS_2 -> POS_3 -> espera -> POS_2
movimiento directo hacia POS_1, POS_2 y POS_3
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
Pulsador DANGER
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

Pulsador DANGER:
  GPIO14 con INPUT_PULLUP
  NORMAL = LOW
  ACTIVO = HIGH
  Se atiende solamente en DEV_READY, con la puerta centrada en POS_2.

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
  Coordinador del producto y FSM superior.
  BOOT, CENTERING, READY, OPENING, OPEN_WAIT, CLOSING, DANGER y STOPPED.
  Crea los CTimer de espera y DANGER y consulta la copia RAM de CDoorConfig.
  Procesa pedidos runtime e inyecta callbacks físicos.

CDoorConfig
  Protocolo JSON por Serial.
  Copia RAM de configuración.
  Persistencia NVS.
  Requests pendientes para el main.

CDoorMotion
  Máquina de estados de posicionamiento.
  START -> MOVING -> SETTLING -> IDLE.
  Llegada, cruce, timeout, stall, cancelación y summary.
  Publica un evento final de éxito o cancelación para la FSM superior.

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

CButton
  Lectura y antirebote no bloqueante del pulsador mediante CTimer.
  El objeto se crea en motorized_door.ino.

CTimer
  Base temporal no bloqueante del firmware.
```

`HOLDING` permanece reservado. El prototipo actual no tiene traba, solenoide, embrague ni mecanismo real de retención que permita validar ese estado de manera concluyente.

---

## FSM superior de demo

En `st_mode=0`, el arranque usa la posición absoluta del AS5048A:

```text
BOOT
  -> si ya está dentro de auto_tolerance_deg de POS_2: READY
  -> si está fuera: CENTERING -> POS_2 -> READY
```

Ciclo `fwd`:

```text
READY -> OPENING_FWD -> POS_1 -> OPEN_WAIT -> CLOSING_CENTER -> POS_2 -> READY
```

Ciclo `rew`:

```text
READY -> OPENING_REW -> POS_3 -> OPEN_WAIT -> CLOSING_CENTER -> POS_2 -> READY
```

`open_wait_ms` se carga desde NVS a la copia RAM de `CDoorConfig`. Al entrar en `OPEN_WAIT`, la FSM toma una copia del valor RAM y usa un `CTimer` creado en `motorized_door.ino`. No se agregó ningún `delay()` para el ciclo.

`stop` cancela el movimiento y el ciclo, deja el equipo en `STOPPED` y no inicia un retorno oculto. La recuperación explícita es mover a POS_2 con `go` o reiniciar en modo normal.

En `st_mode=100` no se ejecuta el centrado automático y se rechazan `fwd`/`rew`; siguen disponibles los comandos de diagnóstico.

### Estado DANGER

El pulsador se evalúa exclusivamente en el estado funcional de reposo:

```text
DEV_READY
puerta centrada en POS_2
motor detenido
LED azul respirando
```

Transición:

```text
DEV_READY + pulsación válida
  -> DEV_DANGER
  -> rojo intermitente durante danger_time_ms
  -> DEV_READY
```

Durante apertura, espera abierta, cierre, centrado o `STOPPED`, el pulsador no genera ninguna transición. `CButton` aplica el antirebote con su propio `CTimer`; el objeto `DangerButton` y el `dangerTimer` del estado se crean en `motorized_door.ino`.

El GPIO14 deja de ser consumido directamente por `CDoorMotion`: ya no cancela movimientos ni fuerza una alarma global. La decisión funcional pertenece únicamente a la FSM superior.

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

Comportamiento de `v5.2a`:

```text
LED_STRIP_OFF
  Tira apagada.

LED_STRIP_IDLE
  Azul con respiración.

LED_STRIP_MOVING_FWD / LED_STRIP_MOVING_RWD
  Verde desplazándose durante apertura, centrado y movimientos directos.

LED_STRIP_OPEN_WAIT
  Verde fijo durante open_wait_ms.

LED_STRIP_CLOSING_FWD / LED_STRIP_CLOSING_RWD
  Rojo desplazándose durante el retorno a POS_2.

LED_STRIP_ARRIVED
  Conserva brevemente el último cuadro antes de volver a IDLE.

LED_STRIP_ALARM
  Rojo intermitente durante DEV_DANGER.
```

La FSM superior decide cuándo corresponde apertura, espera o cierre. `DoorMotion` y `DoorMotor` no conocen colores ni estados de producto.

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

## Defaults compilados de `v5.2b`

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

#define DOOR_OPEN_WAIT_MS_DEFAULT               2000UL
#define DOOR_OPEN_WAIT_MS_MAX                   60000UL
#define DOOR_DANGER_TIME_MS_DEFAULT             3000UL
#define DOOR_DANGER_TIME_MS_MAX                 60000UL

#define DOOR_LOG_LEVEL_DEFAULT                  DOOR_LOG_LEVEL_MSG
```

No se cambia `DOOR_CFG_SCHEMA_VERSION`.

Consecuencia:

```text
La configuración NVS existente se conserva.
Las claves nuevas usan sus defaults si todavía no existen:
  openwait=2000 ms
  dangertime=3000 ms
Al configurar open_wait_ms o danger_time_ms se actualizan RAM y NVS.
factory-reset carga y guarda todos los defaults.
```

---

## Comandos JSON

### Información

```json
{"info":"version"}
{"info":"all-params"}
```

### Ciclo automático de demo

```json
{"cmd":"fwd"}
{"cmd":"rew"}
{"open_wait_ms":2000}
{"danger_time_ms":3000}
```

`open_wait_ms` y `danger_time_ms` aceptan de 0 a 60000 ms y persisten en NVS.

### Movimiento real de diagnóstico

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

`st_mode=0` habilita el centrado automático y los ciclos de demo. `st_mode=100` inicia detenido, evita movimientos automáticos al boot y mantiene disponibles `go`, `stop` y `led-sim`. Cambiar `st_mode` en runtime afecta los próximos comandos; el centrado de arranque se evalúa al reiniciar.

### Restaurar defaults

```json
{"cmd":"factory-reset"}
```

En `v5.2b`, `factory-reset` recupera el set validado, `open_wait_ms=2000`, `danger_time_ms=3000` y `st_mode=0`. No inicia un movimiento inmediatamente; el centrado normal se evalúa en el próximo reinicio.

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

Orden acordado después de validar `v5.2b-danger-button-fsm` en hardware:

```text
1. Validar DANGER únicamente en DEV_READY/POS_2.
2. Cerrar y etiquetar la FSM automática de demo con el evento de pulsador.
3. Mantener como evolución futura el tratamiento del evento en otros estados.
4. Migrar posteriormente a ESP-IDF/RTOS con productor de entrada y cola de eventos.
5. Mantener HOLDING y buzzer fuera hasta contar con hardware justificable.
6. Retomar beam_app sin mezclar sus cambios con el firmware.
```

El informe final presentará el prototipo como un proceso real de I+D, conectando electrónica, firmware, control automático, mecánica, instrumentación, programación, análisis de datos y gestión de riesgo técnico.
