# `motorized_door` - Firmware modular con PID de posicionamiento

Firmware Arduino para un controlador de puerta motorizada basado en ESP32-S3, DRV8833, motor N20 y sensor absoluto AS5048A por SPI.

Esta carpeta contiene la evolución del firmware modular usado para validar el control de posición de la puerta. El branch actual cierra la etapa de **posicionamiento PID** con la versión:

```text
v4.1c-pid-moving-no-hold
```

El objetivo de esta etapa no fue construir todavía el producto final completo, sino demostrar una evolución técnica ordenada:

```text
hardware validado -> código modular -> configuración persistente -> identificación de planta -> simulación -> PID validado en hardware
```

---

## Estado actual del branch

El branch de control de posición queda cerrado con:

```text
motion_mode=2
PID de posición durante MOVING
integral limitada
sin HOLDING activo
flujo: START -> MOVING -> SETTLING -> IDLE
```

Resultado principal:

```text
PID moving v4.1c aceptado.
Sin carga: OK.
Carga práctica 8 g: OK.
Carga práctica 14 g: OK.
Carga extrema 20 g: documentada como ensayo fuera de rango.
```

El siguiente paso del proyecto no es seguir agregando lógica de holding en este branch, sino pasar al **control de dispositivo**, es decir, a la máquina superior que coordina el comportamiento completo del producto.

---

## Hardware validado

El hardware de base fue validado inicialmente con código monolítico y luego conservado durante la modularización.

```text
ESP32-S3 Dev Module
DRV8833
Motor N20 con reductora, alimentación VM=6 V
Sensor absoluto AS5048A por SPI
Final de carrera FC_L
Mecánica/prototipo impreso 3D
```

Conexiones principales:

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
  VDD  = 5V0
  GND  = común

FC_L:
  GPIO14 con INPUT_PULLUP
  NC a GND
  NORMAL = LOW
  ACTIVO = HIGH
```

Posiciones calibradas:

```text
POS_1 = 2.29°
POS_2 = 291.23°
POS_3 = 206.06°
```

Sentidos lógicos validados:

```text
RIGHT / REWIND  -> baja ángulo
LEFT  / FORWARD -> sube ángulo
```

---

## Evolución resumida

### 1. Código monolítico: validación de hardware y mecánica

La primera etapa permitió validar el conjunto físico completo: ESP32-S3, driver DRV8833, motor N20, sensor AS5048A, final de carrera y mecanismo impreso 3D.

En esa versión se confirmó que el problema principal no era de lectura ni de tiempo de CPU. El sensor SPI y el ciclo de control cumplían tiempos muy por debajo del período de control. El problema relevante aparecía en la dinámica física: inercia, rozamiento, carga, sentido y posición de arranque.

El informe asociado es:

```text
../doc/informe_v31_pwm_carga_pid.pdf
```

Ese documento justifica por qué el PWM fijo en lazo abierto no alcanza para una solución robusta: un PWM bajo puede no vencer el arranque, mientras que un PWM alto aumenta el sobrepaso.

### 2. Código modular y responsabilidades separadas

Después de validar el hardware, el firmware evolucionó hacia una arquitectura modular.

Responsabilidades principales:

```text
motorized_door.ino
  Orquesta el producto.
  Crea objetos físicos.
  Inyecta dependencias.
  Procesa requests pendientes.
  Mantiene el estado superior mínimo del dispositivo.

CDoorConfig
  Maneja comunicación JSON por Serial.
  Maneja parámetros persistentes en NVS.
  Expone requests pendientes hacia el .ino.
  Es el punto fuerte de la arquitectura: separa comunicación, memoria y configuración de la lógica física.

CDoorMotion
  Máquina de estados de posicionamiento.
  Implementa START, MOVING, SETTLING e IDLE.
  Maneja llegada, timeout, stall, cancelación y resumen final.
  Selecciona estrategia según motion_mode.

CDoorMotor
  Abstracción del DRV8833 y PWM.

CDoorAngleSensor
  Wrapper conservador del AS5048A.

CLog
  Mensajes humanos, salida de diagnóstico y modo plotter.
```

La decisión más importante fue separar el problema en capas: configuración/comunicación, posicionamiento, motor, sensor y luego una futura máquina superior del dispositivo.

### 3. Perfil no-PID y justificación de control cerrado

Se agregó `motion_mode=1`, un perfil approach no-PID con boost de arranque y PWM lento cerca del objetivo.

Ese paso mostró que se podía mejorar la confiabilidad frente al PWM fijo, sobre todo con carga, pero también dejó claro el costo: empezaban a crecer los parámetros empíricos.

Informe asociado:

```text
../doc/informe_step8A_approach_pid.pdf
```

Lectura técnica: el approach es una transición útil, pero no resuelve de forma general el problema físico. Si cambia la carga o el rozamiento, hay que volver a interpretar y ajustar.

### 4. Identificación de planta

Para no implementar PID a ciegas, se instrumentó el firmware con `motion_mode=0` y PWM fijo para levantar curvas reales de movimiento.

Informe asociado:

```text
../doc/esp32_motorized_door_identificacion_planta_step8F.pdf
```

Modelo promedio identificado:

```text
velocidad_deg_s ≈ 4.97 * (PWM - 18.0)
```

La planta se comportó casi lineal una vez iniciado el movimiento. La no linealidad fuerte apareció en el arranque: PWM 70 podía mover bien una vez que despegaba, pero podía quedar cerca del límite de arranque en algunas condiciones.

Esta etapa fue clave porque permitió pasar de parámetros puramente empíricos a una base medible para simulación.

### 5. Simulación y constantes iniciales

A partir del modelo identificado se simuló un controlador PD/PID en Python.

La simulación propuso una base conservadora:

```text
pid_kp ≈ 0.70
pid_ki = 0.00 inicialmente
pid_kd ≈ 0.05
pid_pwm_max = 80
pid_pwm_min_effective ≈ 55
pid_min_effective_error_deg ≈ 6
pid_i_active_error_deg ≈ 35
pid_integral_limit = 120
```

Al llevarlo al hardware, se verificó que la simulación acertó el orden de magnitud de:

```text
Kp
Kd
PWM máximo
```

La diferencia apareció en la fricción y el arranque reales. El hardware obligó a recalibrar la zona no lineal.

---

## PID v4.1c validado en hardware

Versión cerrada:

```text
v4.1c-pid-moving-no-hold
```

Parámetros aceptados en hardware:

```text
pid_kp = 0.70
pid_ki = 0.05
pid_kd = 0.05
pid_pwm_max = 80
pid_pwm_min_effective = 75
pid_min_effective_error_deg = 2
pid_i_active_error_deg = 20
pid_integral_limit = 120
```

Comparación contra simulación inicial:

| Parámetro | Simulación inicial | Hardware v4.1c | Lectura |
|---|---:|---:|---|
| `pid_kp` | 0.70 | 0.70 | Se mantuvo. Buena escala proporcional. |
| `pid_ki` | 0.00 | 0.05 | Se agregó para vencer error fino por fricción. |
| `pid_kd` | 0.05 | 0.05 | Se mantuvo. Aporta amortiguamiento. |
| `pid_pwm_max` | 80 | 80 | Se mantuvo como límite seguro. |
| `pid_pwm_min_effective` | 55 | 75 | El hardware exigió más PWM para vencer arranque/fricción. |
| `pid_min_effective_error_deg` | 6 | 2 | Se bajó para no forzar PWM mínimo tan cerca del target. |
| `pid_i_active_error_deg` | 35 | 20 | Integral acotada a la zona de llegada. |
| `pid_integral_limit` | 120 | 120 | Se mantuvo; con Ki=0.05 aporta solo ±6. |

Lectura central:

```text
La simulación fue exitosa como punto de partida.
El hardware ajustó las no linealidades reales.
La arquitectura no se cambió: se recalibraron parámetros expuestos por JSON/NVS.
```

Informe asociado:

```text
../doc/informe_pid_v4_1c_simulacion_carga.pdf
```

---

## Resultados de validación v4.1c

### Sin carga

```text
POS_3 -> posicion_alcanzada, final_error = -0.59°, stall = 0/3
POS_1 -> posicion_alcanzada, final_error = -0.42°, stall = 0/3
POS_2 -> posicion_alcanzada, final_error = -1.28°, stall = 0/3
POS_3 -> posicion_alcanzada, final_error = -1.08°, stall = 0/3
```

### Carga aproximada 8 g

```text
POS_1 -> posicion_alcanzada, final_error = -1.56°, stall = 0/3
POS_2 -> posicion_alcanzada, final_error = -1.83°, stall = 0/3
POS_3 -> posicion_alcanzada, final_error =  1.03°, stall = 0/3
POS_1 -> posicion_alcanzada, final_error = -1.61°, stall = 0/3
```

### Carga aproximada 14 g

```text
POS_1 -> posicion_alcanzada, final_error = -1.59°, stall = 0/3
POS_2 -> posicion_alcanzada, final_error = -1.08°, stall = 0/3
POS_3 -> posicion_alcanzada, final_error =  0.99°, stall = 0/3
POS_1 -> posicion_alcanzada, final_error = -1.54°, stall = 0/3
```

### Carga aproximada 20 g: prueba fuera de rango

La carga de 20 g se aplicó con clip e imanes. No fue una carga limpia ni calibrada. El montaje introdujo:

```text
masa adicional
brazo de palanca variable
posible rozamiento
posible balanceo
imanes cerca del sensor magnético AS5048A
```

Por eso se considera una prueba excesiva fuera de rango, útil como ensayo de estrés pero no como especificación de producto.

Resultado resumido:

```text
POS_3 -> sin_movimiento_detectado, final_error =  9.21°, stall = 3/3
POS_2 -> posicion_alcanzada,     final_error = -1.52°, stall = 0/3
POS_1 -> posicion_alcanzada,     final_error = -1.37°, stall = 0/3
POS_2 -> posicion_alcanzada,     final_error = -1.17°, stall = 0/3
POS_3 -> sin_movimiento_detectado, final_error = 1.87°, stall = 3/3
```

Lectura:

```text
20 g no invalida el PID.
Muestra el borde del sistema bajo un montaje desfavorable e inestable.
El sistema operó parcialmente y las fallas se concentraron en POS_3.
```

---

## Comandos principales

### Ver versión

```json
{"info":"version"}
```

### Ver parámetros

```json
{"info":"all-params"}
```

### Activar PID de posición

```json
{"motion_mode":2}
{"pid_kp":0.7}
{"pid_ki":0.05}
{"pid_kd":0.05}
{"pid_pwm_max":80}
{"pid_pwm_min_effective":75}
{"pid_min_effective_error_deg":2}
{"pid_i_active_error_deg":20}
{"pid_integral_limit":120}
```

### Movimiento

```json
{"cmd":"go","pos":1}
{"cmd":"go","pos":2}
{"cmd":"go","pos":3}
```

### Cancelación

```json
{"cmd":"stop"}
```

---

## Modos de movimiento

```text
motion_mode=0
  PWM fijo.
  Baseline para comparación e identificación de planta.

motion_mode=1
  Approach no-PID.
  Usa boost de arranque y PWM lento cerca del objetivo.

motion_mode=2
  PID de posición durante movimiento.
  Versión actual validada: v4.1c-pid-moving-no-hold.
```

---

## Decisión sobre HOLDING

Aunque la FSM ya fue preparada conceptualmente para un futuro estado `HOLDING`, en esta etapa se decidió no implementar un HOLDING activo.

Motivo:

```text
El prototipo actual no tiene traba mecánica.
No tiene solenoide.
No tiene embrague ni liberación por fuerza.
No hay medición repetible de perturbación externa.
No se puede validar de forma concluyente un mantenimiento activo de posición.
```

En un equipo real, la retención frente a movimiento no autorizado debería resolverse con hardware mecánico adecuado:

```text
bloqueo mecánico
solenoide
embrague o liberación por fuerza
protecciones de usuario durante movimiento
```

El PID debe entenderse como control de posición y corrección dinámica, no como sustituto de la seguridad mecánica.

Por eso este branch se cierra con:

```text
PID moving validado
HOLDING reservado para etapa futura con hardware adecuado
```

---

## Cierre del branch

Este branch deja validado el subsistema de posicionamiento:

```text
sensor absoluto operativo
motor y driver validados
mecánica impresa 3D validada como prototipo
arquitectura modular funcional
configuración JSON/NVS operativa
motion_mode=0 conservado como baseline
motion_mode=1 conservado como antecedente no-PID
motion_mode=2 validado como PID moving
```

Conclusión técnica:

```text
El proyecto demostró el recorrido completo desde una solución monolítica con PWM fijo hasta un control PID calibrado por simulación y validado en hardware real.
```

Esta etapa queda guardada para continuar desde `main` con el control superior del dispositivo.

---

## Próximo branch: control de dispositivo

El próximo trabajo debería salir del control fino de posición y pasar a la máquina superior del producto.

Objetivo:

```text
separar claramente estado del dispositivo y estado de posicionamiento
```

Estados candidatos para la máquina superior:

```text
BOOT
IDLE
READY
MOVING
POSITION_REACHED
ERROR
SERVICE / CALIBRATION
```

`CDoorMotion` queda como subsistema ya validado para mover a posiciones. La nueva capa debe decidir cuándo aceptar comandos, cuándo bloquearlos, cómo reportar estado global y cómo evolucionar hacia un diseño más cercano a producto.

---

## Informes de referencia

Desde esta carpeta, los informes se encuentran en `../doc/`:

```text
../doc/informe_v31_pwm_carga_pid.pdf
../doc/informe_step8A_approach_pid.pdf
../doc/esp32_motorized_door_identificacion_planta_step8F.pdf
../doc/informe_pid_v4_1c_simulacion_carga.pdf
```

Lectura sugerida:

```text
1. informe_v31_pwm_carga_pid.pdf
   Valida hardware, sensor, driver, tiempos de control y límites del PWM fijo.

2. informe_step8A_approach_pid.pdf
   Explica el perfil approach no-PID y por qué mejora algunos casos pero aumenta parámetros empíricos.

3. esp32_motorized_door_identificacion_planta_step8F.pdf
   Levanta curvas reales, identifica la planta y obtiene el modelo velocidad_deg_s ≈ 4.97 * (PWM - 18.0).

4. informe_pid_v4_1c_simulacion_carga.pdf
   Documenta simulación, ajuste en hardware, PID v4.1c y pruebas de carga.
```

---

## Nota final

Este firmware no es solamente un ejemplo de mover un motor. Es una demostración completa de evolución técnica:

```text
validar hardware
medir límites físicos
modularizar responsabilidades
persistir configuración
levantar planta
simular control
implementar PID
validar con carga
cerrar el subsistema de posicionamiento
preparar la máquina superior del dispositivo
```

El valor principal del proyecto está en mostrar por qué un sistema físico real necesita pasar de PWM empírico a control cerrado medido, sin perder una arquitectura clara y mantenible.
