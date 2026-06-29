# Step 8F - Identificación de planta con `travel` plotter

Versión validada:

```text
v3.1-continuous-silent-measured-config-json-door-motion-motor-sensor-log-step8F-plant-travel-plotter
```

## Objetivo de la etapa

Esta etapa tiene como objetivo levantar curvas reales de movimiento del sistema para identificar un modelo simple de planta antes de implementar un controlador PD/PID.

El sistema físico utilizado es:

```text
ESP32-S3 + DRV8833 + motor N20 + AS5048A
```

La idea central es medir cómo responde la puerta ante un PWM fijo, usando la arquitectura modular ya validada, sin modificar todavía la lógica de control.

El objetivo no es implementar PID en firmware en esta etapa, sino obtener datos experimentales para:

```text
1. Medir velocidad angular aproximada.
2. Estimar relación PWM -> velocidad.
3. Detectar zona muerta y problemas de arranque.
4. Comparar comportamiento por sentido.
5. Simular en Python un futuro control PD/PID.
6. Obtener constantes iniciales conservadoras.
```

---

## Principio de trabajo

El sistema actual funciona y se mantiene como baseline estable.

Por eso, en esta etapa se siguió el criterio:

```text
Cambios mínimos.
Cambios conservadores.
No tocar motor.
No tocar sensor.
No tocar FSM de posicionamiento.
No tocar llegada, stall ni timeout.
No implementar PID todavía.
```

La instrumentación se limitó a generar una salida limpia para el Serial Plotter de Arduino.

---

## Arquitectura actual

El firmware mantiene la separación de responsabilidades:

```text
motorized_door.ino
  Coordina el producto.
  Mantiene DeviceState.
  Crea e inyecta objetos físicos.
  Procesa requests pendientes de Config.
  Contiene Log.
  No contiene lógica fina de movimiento.

door_config
  Protocolo JSON host.
  Parámetros persistentes en NVS.
  Requests pendientes.
  No toca lógica física ni FSM de movimiento.

door_motion
  FSM de posicionamiento automático.
  Llegada, cruce, stall, timeout, settle final, cancelación y summary.
  Maneja la estrategia de movimiento según motion_mode.
  No contiene Log.

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

## Instrumentación Step 8F

Para identificación de planta se usa únicamente:

```text
motion_mode = 0
```

Es decir, PWM fijo en lazo abierto.

El modo `motion_mode=1` queda como antecedente comparativo de Step 8A, pero no se usa para calcular constantes de planta porque introduce boost, zona lenta y perfil de acercamiento.

---

## Variables del Plotter

La salida para el Serial Plotter se redujo intencionalmente a:

```text
travel
target
```

Ejemplo:

```text
travel:0.00    target:157.06
travel:1.16    target:157.06
travel:11.47   target:157.06
travel:23.95   target:157.06
...
```

Significado:

```text
travel
  Recorrido angular realizado desde el inicio del movimiento.

target
  Recorrido angular total necesario para llegar al objetivo.
```

Se evitó graficar:

```text
angle absoluto
error firmado
arrival_tol
pwm_cmd
```

Motivo:

```text
angle absoluto
  Confunde por el wrap 0/360 del sensor absoluto.

error firmado
  Ensucia la lectura principal para identificación de planta.

arrival_tol y pwm_cmd
  No son necesarios en el plotter principal.
  El PWM queda registrado por el ensayo.
```

---

## Comandos base de ensayo

Secuencia utilizada para configurar PWM fijo e iniciar una corrida:

```json
{"log_level":1}
{"motion_mode":0}
{"pwm_move":70}
{"info":"all-params"}
{"log_level":3}
{"cmd":"go","pos":3}
```

Luego se repite cambiando PWM y posición destino:

```json
{"log_level":1}
{"pwm_move":75}
{"info":"all-params"}
{"log_level":3}
{"cmd":"go","pos":1}
```

```json
{"log_level":1}
{"pwm_move":80}
{"info":"all-params"}
{"log_level":3}
{"cmd":"go","pos":3}
```

Para que el plotter quede limpio, conviene abrirlo o limpiarlo después de los comandos JSON de configuración y antes de iniciar el movimiento con `log_level=3`.

---

## Ensayos levantados

Se levantaron curvas con PWM fijo para recorridos cercanos entre POS_1 y POS_3.

Posiciones calibradas:

```text
POS_1 = 2.29°
POS_2 = 291.23°
POS_3 = 206.06°
```

Recorridos principales:

```text
POS_1 -> POS_3
POS_3 -> POS_1
```

Ambos recorridos están cerca de 157-160°, lo que permite comparar curvas de forma razonable.

---

## Resultados experimentales

Tabla resumen:

| PWM | Sentido | Target aproximado | Velocidad útil | Delay de arranque | Resultado |
|---:|---|---:|---:|---:|---|
| 70 | POS_1 -> POS_3 | 156.97° | ~258 °/s | ~50 ms | Llega bien |
| 70 | POS_3 -> POS_1 | 159.53° | ~258 °/s | ~950 ms | Llega, pero arranque marginal |
| 75 | POS_1 -> POS_3 | 157.06° | ~282 °/s | ~50 ms | Llega bien |
| 75 | POS_3 -> POS_1 | 159.53° | ~286 °/s | ~50 ms | Llega bien |
| 80 | POS_1 -> POS_3 | 157.37° | ~307 °/s | ~50 ms | Llega bien |
| 80 | POS_3 -> POS_1 | 159.50° | ~309 °/s | ~50 ms | Llega bien |

---

## Observaciones principales

Las curvas muestran que, una vez iniciado el movimiento, el crecimiento de `travel` es casi lineal.

Esto permite modelar la planta, en primera aproximación, como:

```text
PWM fijo -> velocidad angular aproximadamente constante -> posición/travel
```

El comportamiento observado es compatible con un modelo simple:

```text
velocidad = Kv * (PWM - PWM_dead)
```

---

## Modelo estimado de planta

Promediando ambos sentidos:

```text
PWM 70 -> ~258.4 °/s
PWM 75 -> ~284.1 °/s
PWM 80 -> ~308.1 °/s
```

Modelo lineal estimado:

```text
velocidad_deg_s ≈ 4.97 * (PWM - 18.0)
```

Constantes iniciales:

```text
Kv ≈ 4.97 °/s/PWM
PWM_dead_dinámico ≈ 18
```

Separado por sentido:

```text
POS_1 -> POS_3:
  velocidad ≈ 4.89 * (PWM - 17.2)

POS_3 -> POS_1:
  velocidad ≈ 5.06 * (PWM - 18.8)
```

La diferencia por sentido es baja para esta primera identificación, por lo que para la primera simulación se puede usar el modelo promedio.

---

## Punto crítico detectado: arranque

El dato más importante de la tanda es que PWM 70 no es marginal por velocidad una vez que el motor se mueve, pero sí puede ser marginal para iniciar el movimiento en cierto sentido.

Caso observado:

```text
PWM 70, POS_3 -> POS_1:
  delay de arranque ≈ 950 ms
  velocidad una vez iniciado ≈ 258 °/s
  llegada correcta
```

Esto separa dos fenómenos distintos:

```text
1. Velocidad en régimen:
   Bastante lineal con el PWM.

2. Arranque:
   Puede depender de fricción estática, posición, carga, sentido y alimentación.
```

Conclusión:

```text
PWM 70 alcanza para mover una vez que el sistema despegó,
pero puede quedar cerca del límite de arranque.
```

PWM 75 y PWM 80 arrancaron de forma mucho más confiable en los ensayos realizados.

---

## Relación con Step 8A

Step 8A había incorporado `motion_mode=1`, un perfil no-PID con:

```text
pwm_start
pwm_move
pwm_slow
slow_zone_deg
start_boost_ms
```

Ese perfil mejoraba algunos casos de arranque y acercamiento, pero agregaba más parámetros empíricos.

Step 8F confirma experimentalmente por qué ese perfil ayudaba:

```text
El sistema puede necesitar más PWM al inicio para vencer fricción estática,
pero no necesariamente necesita ese mismo PWM durante todo el recorrido.
```

Sin embargo, seguir agregando parámetros empíricos no es la solución definitiva. La identificación de planta permite avanzar hacia una estrategia más fundamentada.

---

## Implicancia para futuro PD/PID

El futuro controlador no debería diseñarse a ciegas.

A partir de estas curvas, el modelo inicial de simulación puede ser:

```text
error = target - travel
velocidad = d(travel) / dt
PWM_control = Kp * error + Ki * integral - Kd * velocidad
```

Para comenzar, conviene simular primero un PD:

```text
Ki = 0 inicialmente
```

La parte proporcional cumple un rol importante al inicio:

```text
Cuando el error es grande, Kp * error tiende a pedir mucho PWM.
En la práctica, esa salida se saturará al PWM máximo permitido.
Esto ayuda a iniciar el movimiento y recorrer rápido hacia el objetivo.
```

Cerca del setpoint, el error disminuye:

```text
La contribución proporcional baja.
La parte derivativa puede ayudar a frenar según la velocidad.
La parte integral, si se usa, debe entrar de forma moderada para compensar error residual,
rozamiento o carga, y ayudar a mantenerse cerca del punto.
```

Es decir:

```text
P:
  Empuja fuerte cuando el error es grande.
  En arranque puede llevar la salida al máximo configurado.

D:
  Ayuda a frenar en función de la velocidad.
  Puede reducir sobrepaso.

I:
  No debería usarse para forzar el arranque.
  Sirve para corregir error residual y mantenerse cerca del punto.
  Debe ser pequeña y controlada para evitar acumulación excesiva.
```

---

## Criterio conservador para el próximo paso

No implementar PID todavía en firmware.

Secuencia recomendada:

```text
1. Usar las curvas levantadas.
2. Simular la planta en Python.
3. Probar control PD con saturación de PWM.
4. Agregar Ki solo si queda error estacionario relevante.
5. Verificar sobrepaso, tiempo de llegada y estabilidad.
6. Obtener constantes iniciales conservadoras.
7. Recién después pensar implementación en firmware como motion_mode=2.
```

---

## Documentación generada

Se generó un informe con curvas, tablas y conclusiones:

```text
esp32_motorized_door_identificacion_planta_step8F.pdf
```

El informe incluye:

```text
Curvas POS_1 -> POS_3.
Curvas POS_3 -> POS_1.
Comparación de todas las curvas.
Delay de arranque.
Modelo velocidad vs PWM.
Tabla de identificación.
Conclusiones para futura simulación PD/PID.
```

---

## Conclusión técnica

Step 8F valida que el sistema puede identificarse con un modelo simple de velocidad en régimen:

```text
velocidad_deg_s ≈ 4.97 * (PWM - 18.0)
```

La planta se comporta de forma casi lineal para PWM fijo una vez iniciado el movimiento.

La principal no linealidad observada no está en la velocidad de régimen, sino en el arranque:

```text
PWM 70 puede ser suficiente para moverse,
pero no siempre es suficiente para despegar rápidamente.
```

Esto justifica avanzar hacia un control cerrado PD/PID, pero manteniendo una estrategia conservadora:

```text
Primero simulación.
Después constantes iniciales.
Después implementación controlada en firmware.
```

El proyecto queda preparado para explicar de forma práctica por qué un sistema real con motor, carga, inercia, rozamiento y sensor absoluto de posición necesita pasar de PWM empírico a identificación de planta y control cerrado.