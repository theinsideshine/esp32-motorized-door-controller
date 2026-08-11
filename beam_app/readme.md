# Door Controller App

Aplicación desktop en Python/PySide6 para operar y diagnosticar el controlador de una puerta motorizada mediante un ESP32.

## Funciones de la demo

- Conexión serie real o simulación local.
- Lectura de versión, configuración y posición angular absoluta.
- Ciclos `FWD`, `REW` y parada `STOP`.
- Movimientos directos a `POS_1`, `POS_2` y `POS_3`.
- Edición y confirmación de `pos1_deg`, `pos2_deg`, `pos3_deg` y `pwm_move`.
- Visualización de los eventos estructurados `ack`, `motion-complete` y `cycle-complete`.

## Arquitectura

```text
door_controller_app/
├── main.py
├── controllers/
│   └── main_controller.py
├── core/
│   ├── serial_manager.py
│   └── serial_event_queue.py
├── ui/
│   ├── main_window.py
│   └── door_position_widget.py
├── config/
│   └── simulation_data.json
├── requirements.txt
├── run.bat
├── run_first.bat
└── run.sh
```

La lectura serie se mantiene desacoplada de Qt: `SerialManager` produce líneas en `SerialEventQueue` y `MainController` consume la cola desde un `QTimer` en el hilo de la interfaz.

## Instalación

Windows:

```bat
cd door_controller_app
run_first.bat
```

Linux:

```bash
cd door_controller_app
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Ejecución

Windows:

```bat
run.bat
```

Linux:

```bash
./run.sh
```

Ejecución manual desde la raíz:

```bash
python main.py
```

## Protocolo usado en la demo

La aplicación solicita los valores confirmados con:

```json
{"info":"all-params"}
```

Las posiciones, PWM y movimientos se envían con los mensajes definidos por el firmware:

```json
{"pos1_deg":2.29}
{"pos2_deg":291.23}
{"pos3_deg":206.06}
{"pwm_move":80}
{"cmd":"go","pos":1}
{"cmd":"go","pos":2}
{"cmd":"go","pos":3}
{"cmd":"fwd"}
{"cmd":"rew"}
{"cmd":"stop"}
```

En modo real, los campos no se consideran confirmados hasta recibir nuevamente `all-params`. Los logs humanos del firmware se muestran en consola, pero no se usan para reconstruir estados.
