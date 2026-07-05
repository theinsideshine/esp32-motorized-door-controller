# esp32-motorized-door-controller

Este repositorio documenta la evolución de un controlador de posición para una puerta motorizada basada en ESP32.

El proyecto no busca solamente mover un motor. El objetivo principal es mostrar una evolución técnica completa: validar hardware, probar una mecánica impresa en 3D, medir el comportamiento real del sistema, comparar estrategias de control y ordenar el firmware en una arquitectura cada vez más mantenible.

El recorrido parte de un sketch Arduino monolítico usado para validar el driver de motor, el sensor de posición y la lógica básica de movimiento. Luego evoluciona hacia un firmware modular, con configuración persistente, comunicación JSON, máquinas de estado no bloqueantes, identificación de planta, simulación y control PID de posición.

## Objetivos del proyecto

- Validar un sistema real con ESP32-S3, DRV8833, motor N20 y sensor absoluto AS5048A.
- Probar la mecánica e impresión 3D del mecanismo antes de complejizar el firmware.
- Comparar control bloqueante contra control no bloqueante.
- Comparar un sketch monolítico contra una arquitectura modular con responsabilidades separadas.
- Comparar PWM fijo, perfiles de movimiento no-PID y control cerrado PID.
- Usar comunicación JSON y configuración persistente en NVS.
- Identificar la planta real desde ensayos con PWM fijo.
- Simular el comportamiento del sistema antes de llevar constantes al firmware.
- Cerrar una etapa de control de posición antes de avanzar hacia una máquina superior de dispositivo.
- Preparar conceptualmente una futura migración hacia ESP-IDF y arquitectura basada en eventos.

## Estructura del repositorio

```text
esp32-motorized-door-controller/
├── doc/
├── motor_door_wpid/
├── motorized_door/
├── simulacion/
├── .gitignore
└── readme.md
```

### `motor_door_wpid/`

Carpeta histórica de las primeras versiones del firmware. En esta etapa se validaron los aspectos básicos del sistema:

- conexión del ESP32-S3;
- driver DRV8833;
- motor N20;
- sensor absoluto AS5048A por SPI;
- sentidos de giro;
- posiciones calibradas;
- respuesta del mecanismo impreso en 3D;
- movimiento por PWM fijo;
- comportamiento con carga.

Esta parte es importante porque permitió separar problemas de hardware, impresión 3D, sensor y alimentación antes de avanzar hacia arquitectura y control cerrado.

### `motorized_door/`

Firmware modular actual en Arduino.

Esta carpeta contiene la evolución principal del controlador. La versión actual de cierre de esta etapa es:

```text
v4.1c-pid-moving-no-hold
```

El firmware está organizado por responsabilidades:

- `motorized_door.ino`: orquestación general, objetos globales, inicialización y ciclo principal.
- `door_config.*`: configuración, comunicación JSON, persistencia NVS y requests del host.
- `door_motion.*`: máquina de estados de posicionamiento.
- `door_motor.*`: abstracción del DRV8833 y PWM.
- `door_angle_sensor.*`: lectura del AS5048A.
- `log.*`: salida humana, JSON y plotter.

El punto fuerte de esta etapa es la aparición de un objeto de configuración que concentra dos responsabilidades críticas para un firmware real:

1. comunicación con el host mediante JSON;
2. persistencia de parámetros en NVS.

Gracias a eso, la calibración del sistema puede hacerse desde comandos JSON sin recompilar firmware.

### `simulacion/`

Carpeta destinada a scripts y pruebas de simulación.

Después de identificar la planta real con PWM fijo, se usó una simulación para obtener constantes iniciales conservadoras para el futuro PD/PID. La simulación no reemplazó la prueba en hardware, pero permitió partir de valores razonables en vez de ajustar completamente a ciegas.

La práctica confirmó que la simulación fue útil: `Kp`, `Kd` y el límite de PWM quedaron muy cerca de lo esperado. Lo que hubo que corregir en hardware fue la zona no lineal real: fricción, arranque y carga.

### `doc/`

Carpeta de documentación técnica del proyecto.

Los informes documentan la evolución experimental y justifican cada cambio importante.

Documentos principales:

- [`informe_v31_pwm_carga_pid.pdf`](doc/informe_v31_pwm_carga_pid.pdf)  
  Ensayos iniciales con PWM fijo, carga y justificación de avanzar hacia control cerrado.

- [`informe_step8A_approach_pid.pdf`](doc/informe_step8A_approach_pid.pdf)  
  Comparación del perfil approach no-PID contra PWM fijo. Muestra que el approach mejora algunos casos, pero aumenta el universo de parámetros empíricos.

- [`esp32_motorized_door_identificacion_planta_step8F.pdf`](doc/esp32_motorized_door_identificacion_planta_step8F.pdf)  
  Identificación de planta con `motion_mode=0`, PWM fijo y curvas `travel` vs tiempo. Desde allí se obtuvo un modelo inicial de velocidad.

- [`informe_pid_v4_1c_simulacion_carga.pdf`](doc/informe_pid_v4_1c_simulacion_carga.pdf)  
  Cierre de la etapa PID v4.1c: comparación simulación/hardware, ajuste de constantes, pruebas sin carga, con 8 g, 14 g y prueba extrema de 20 g.

## Evolución técnica resumida

### 1. Validación monolítica

La primera etapa se enfocó en comprobar que el hardware y la mecánica funcionaban.

Se validó:

- lectura del AS5048A;
- accionamiento del DRV8833;
- sentidos lógicos de giro;
- posiciones físicas calibradas;
- respuesta del mecanismo impreso en 3D;
- comportamiento con PWM fijo;
- efecto de carga sobre arranque, sobrepaso y stall.

La conclusión fue clara: el sistema leía y decidía a tiempo, pero el resultado final dependía de inercia, rozamiento, sentido, posición inicial y carga.

### 2. Modularización y responsabilidades

Luego el firmware se llevó a una estructura más mantenible.

La arquitectura separa:

- control de motor;
- lectura de sensor;
- configuración;
- comunicación JSON;
- persistencia NVS;
- máquina de estados de posicionamiento;
- logging.

Esto permitió evolucionar sin romper el baseline validado. La comunicación JSON y la persistencia NVS fueron claves porque los parámetros del sistema pudieron ajustarse desde el host.

### 3. PWM fijo y perfil no-PID

El sistema funcionaba con PWM fijo, pero no existía un único valor ideal:

- PWM bajo: menos sobrepaso, pero riesgo de no arrancar.
- PWM alto: mejor arranque, pero más inercia y sobrepaso.
- Carga: puede frenar algunos recorridos, pero también generar stalls.

Luego se probó un perfil approach no-PID, con boost de arranque y reducción de PWM cerca del objetivo. Mejoró algunos casos, pero aumentó la cantidad de parámetros empíricos.

Esa etapa justificó avanzar hacia una estrategia más formal: identificar la planta y simular un PD/PID.

### 4. Identificación de planta

Para identificar la planta se usó exclusivamente:

```text
motion_mode=0
PWM fijo
```

Se midieron curvas con PWM 70, 75 y 80. El comportamiento observado fue casi lineal una vez que el motor estaba en movimiento.

Modelo inicial obtenido:

```text
velocidad_deg_s ≈ 4.97 * (PWM - 18.0)
```

Este modelo fue suficiente para simular un controlador conservador y proponer constantes iniciales.

### 5. Simulación y PID real

La simulación propuso inicialmente una base conservadora:

```text
Kp ≈ 0.70
Ki = 0
Kd ≈ 0.05
PWM max = 80
PWM mínimo efectivo ≈ 55
```

En hardware se verificó que la escala de control era correcta, pero la fricción real exigía ajustes:

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

La diferencia entre simulación y hardware no se tomó como fracaso del modelo, sino como parte normal del proceso: la simulación aproximó bien la dinámica gruesa y el hardware ajustó la zona no lineal.

### 6. Cierre de branch: PID de posicionamiento

La versión `v4.1c-pid-moving-no-hold` valida `motion_mode=2` con PID de posición e integral limitada.

Flujo de movimiento validado:

```text
START -> MOVING -> SETTLING -> IDLE
```

No se implementa `HOLDING` activo en esta etapa. La razón es técnica: el prototipo actual no tiene traba, solenoide, embrague ni medición de fuerza externa. Por lo tanto, un holding activo no tendría una prueba concluyente ni representaría seguridad real de producto.

El criterio adoptado es:

- cerrar el branch de posicionamiento con PID moving validado;
- documentar `HOLDING` como reservado;
- pasar al desarrollo de la máquina superior de dispositivo.

## Resultado de la etapa PID v4.1c

El resultado fue positivo.

La simulación fue un punto de partida exitoso:

- `Kp=0.70` se mantuvo;
- `Kd=0.05` se mantuvo;
- `PWM max=80` se mantuvo.

El hardware corrigió la zona real de fricción:

- `pid_pwm_min_effective` subió de 55 a 75;
- `pid_min_effective_error_deg` bajó de 6 a 2;
- se agregó una integral limitada `Ki=0.05`.

Pruebas realizadas:

- sin carga: correcto;
- 8 g: correcto;
- 14 g: correcto;
- 20 g: documentado como prueba extrema fuera de rango.

La prueba de 20 g se realizó con clip e imanes cerca del sensor, por lo que no representa una carga limpia. Introduce masa, torque variable, posible rozamiento y posible perturbación magnética sobre el AS5048A. Aun así, sirvió como ensayo de borde para observar cómo se degrada el sistema ante una carga excesiva e inestable.

## Lectura para una posible producción

El proyecto muestra que un PID limitado puede absorber variaciones razonables de fabricación:

- pequeñas diferencias de fricción;
- tolerancias de impresión 3D;
- diferencias entre motores;
- pequeñas variaciones de montaje;
- cambios moderados de carga.

Si una unidad real queda más dura o más liviana que otra, no hace falta rediseñar la arquitectura. Se pueden recalibrar parámetros por JSON/NVS, igual que se hizo al pasar de la simulación al hardware.

## Sobre HOLDING y seguridad mecánica

El modo `HOLDING` queda reservado para una etapa posterior.

En un equipo real, mantener una puerta contra un usuario no autorizado no debería depender solo del motor ni del PID. Debería resolverse con elementos mecánicos adecuados:

- solenoide o traba;
- embrague o liberación por fuerza;
- criterio de seguridad durante movimiento;
- detección o limitación ante obstrucciones.

El PID debe posicionar y corregir error fino. No debe reemplazar la seguridad mecánica.

## Próximo paso

La etapa de posicionamiento queda cerrada con:

```text
v4.1c-pid-moving-no-hold
```

El próximo trabajo recomendado es avanzar sobre el control de dispositivo:

```text
device-control-state-machine
```

La idea es separar claramente:

- máquina de estados superior del producto;
- subsistema de posicionamiento;
- comandos del host;
- errores;
- estados de servicio/calibración;
- futuras condiciones de seguridad.

## Estado actual

```text
Hardware validado: sí
Mecánica impresa 3D validada: sí
Firmware modular: sí
Comunicación JSON: sí
Persistencia NVS: sí
Identificación de planta: sí
Simulación PID inicial: sí
PID en hardware: sí
Pruebas de carga práctica: sí
HOLDING activo: reservado
Control de dispositivo: próximo branch
```
