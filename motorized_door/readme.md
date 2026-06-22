## Step 8A - Motion mode configurable y perfil approach no-PID

Versión validada:

```text
v3.1-continuous-silent-measured-config-json-door-motion-motor-sensor-log-step8A-motion-mode
```

### Objetivo de la etapa

Esta etapa agrega un modo de movimiento configurable sin modificar la lógica base de posicionamiento validada en Step 7.

El objetivo no es implementar PID todavía, sino incorporar un perfil no-PID simple que permita comparar:

* PWM fijo en lazo abierto.
* Perfil de arranque y acercamiento al punto.
* Comportamiento sin carga.
* Comportamiento con carga.
* Cantidad de parámetros necesarios cuando se intenta resolver el problema solo con configuración empírica.

La conclusión principal es que el perfil approach puede mejorar la confiabilidad de arranque y reducir algunos errores, pero también aumenta el universo de configuración. Esto justifica avanzar hacia identificación de planta y control cerrado PD/PID.

---

### Arquitectura actual

El firmware mantiene la separación de responsabilidades:

```text
motorized_door.ino
  Coordina el producto.
  Mantiene DeviceState.
  Crea e inyecta objetos físicos.
  Procesa requests pendientes de Config.
  No contiene lógica fina de movimiento.

door_config
  Protocolo JSON host.
  Parámetros persistentes en NVS.
  Requests pendientes.
  No toca lógica física ni FSM de movimiento.

door_motion
  FSM de posicionamiento automático.
  Llegada, cruce, stall, timeout, settle final, cancelación y summary.
  Incorpora selección de estrategia PWM según motion_mode.

door_motor
  Driver DRV8833 / PWM.

door_angle_sensor
  Wrapper conservador del AS5048A.
  El objeto MagneticSensorAS5048A y la inicialización SPI siguen viviendo en el .ino.

log
  Clog para mensajes humanos.
  JSON sigue saliendo por Serial directo.
```

---

### Modos de movimiento

Se agregó el parámetro persistente:

```json
{"motion_mode":0}
```

Valores disponibles:

```text
motion_mode = 0
  Modo simple / fixed PWM.
  Usa pwm_move.
  Conserva el comportamiento validado de Step 7.

motion_mode = 1
  Modo approach / perfil no-PID.
  Usa boost de arranque, PWM base y PWM reducido cerca del objetivo.
```

---

### Nuevos parámetros configurables

Para `motion_mode=1` se agregaron:

```json
{"pwm_start":80}
{"pwm_slow":60}
{"slow_zone_deg":20}
{"start_boost_ms":120}
```

Significado:

```text
pwm_start
  PWM inicial para vencer rozamiento/carga.

pwm_move
  PWM principal del recorrido.

pwm_slow
  PWM reducido cerca del target.

slow_zone_deg
  Zona angular donde se usa pwm_slow.

start_boost_ms
  Tiempo inicial durante el cual se aplica pwm_start.
```

Configuración usada en la validación Step 8A:

```json
{"motion_mode":1}
{"pwm_move":70}
{"pwm_start":80}
{"pwm_slow":60}
{"slow_zone_deg":20}
{"start_boost_ms":120}
```

---

### Validación funcional

Se validó primero `motion_mode=0` para confirmar que el comportamiento de Step 7 seguía intacto.

Luego se probó `motion_mode=1` con la misma secuencia de movimientos:

```text
POS_1 -> POS_2
POS_2 -> POS_3
POS_3 -> POS_2
POS_2 -> POS_1
```

También se probó cancelación por host con:

```json
{"cmd":"stop"}
```

Resultados generales:

```text
motion_mode=0
  Conserva el comportamiento base.
  Reproduce las limitaciones esperadas del PWM fijo.
  PWM 70 sigue cerca del límite de arranque.

motion_mode=1
  No generó stalls en las tandas realizadas.
  Mejoró algunos recorridos, especialmente con carga.
  No eliminó todos los sobrepasos.
  El recorrido POS_1 -> POS_2 sigue siendo uno de los más críticos.
```

---

### Comparación con informe v3.1 monolítico

El informe anterior de v3.1 mostraba que con PWM fijo no existía un único valor que garantizara simultáneamente:

* Arranque confiable.
* Bajo sobrepaso.
* Buen comportamiento con carga.
* Buen comportamiento en ambos sentidos.

Resumen comparativo:

```text
PWM fijo 70, v3.1 monolítica:
  2 g -> stalls puntuales.
  8 g -> stalls puntuales.
  La carga reducía algunos errores, pero aumentaba el riesgo de no arrancar.

Step 8A, motion_mode=1:
  2 g -> sin stalls observados.
  8 g -> sin stalls observados.
  Error promedio comparable o mejor.
  Se mantiene sobrepaso en algunos recorridos.
```

Esto confirma que el perfil approach es una mejora intermedia útil, pero no una solución definitiva.

---

### Documentación generada

Se agregó un informe comparativo para usar como material explicativo:

```text
informe_step8A_approach_pid.docx
informe_step8A_approach_pid.pdf
```

El informe incluye:

* Comparación con v3.1 monolítica.
* Gráficos de error promedio.
* Gráficos de stalls.
* Gráficos de error máximo.
* Error firmado por recorrido.
* Explicación del aumento del universo de configuración.
* Justificación de evolución hacia identificación de planta y PID/PD.

---

### Conclusión técnica

Step 8A demuestra que se puede mejorar el comportamiento del sistema sin cambiar todavía a PID, usando un perfil simple de PWM.

Sin embargo, la mejora requiere nuevos parámetros:

```text
pwm_move
pwm_start
pwm_slow
slow_zone_deg
start_boost_ms
```

Cada cambio mecánico, carga, rozamiento o alimentación puede requerir nuevas inferencias y nuevos ajustes.

Por eso, el siguiente paso conceptual no debería ser seguir acumulando parámetros empíricos, sino avanzar hacia:

```text
1. Levantamiento de curvas de planta.
2. Identificación de constantes dinámicas.
3. Diseño de control PD/PID.
4. Implementación controlada y validada sobre la FSM existente.
```

El proyecto queda preparado para explicar, de forma teórica y práctica, por qué se justifica el uso de PID en un sistema real con motor, carga, inercia, rozamiento y sensor absoluto de posición.
