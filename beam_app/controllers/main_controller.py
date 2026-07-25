import json
from queue import Empty

import serial.tools.list_ports

from PySide6.QtCore import QTimer, QUrl
from PySide6.QtWidgets import QMessageBox
from PySide6.QtGui import QDesktopServices

from config.app_config import GITHUB_URL, SKYCIV_URL, MSJ_INIT_COM


from config.app_config import (
    GITHUB_URL,
    SKYCIV_URL,
    MSJ_INIT_COM,
    STATE_DISCONNECTED,
    STATE_CONNECTED,
    STATE_RUNNING,
    STATUS_BAR_STYLES,
)

from config.app_config import (
    STATE_DISCONNECTED as CFG_STATE_DISCONNECTED,
    STATE_CONNECTED as CFG_STATE_CONNECTED,
    STATE_RUNNING as CFG_STATE_RUNNING,
)

from core.serial_manager import SerialManager
from core.serial_event_queue import SerialEventQueue

class MainController:
    STATE_DISCONNECTED = CFG_STATE_DISCONNECTED
    STATE_CONNECTED = CFG_STATE_CONNECTED
    STATE_RUNNING = CFG_STATE_RUNNING

    def __init__(self, view):
        self.view = view
        self.serial_manager = SerialManager()

        self.serial_queue = SerialEventQueue()
        self.serial_manager.set_event_queue(self.serial_queue)

        self._json_buffer = []
        self._receiving_json = False
        self._pending_start = False
        self._waiting_manual_response = False
        self._auto_refresh_done = False

        self.current_state = self.STATE_DISCONNECTED

        # Handshake de conexión con el banco
        self._waiting_init_serial = False
        self._connect_port_name = ""
        self._connect_timer = QTimer()
        self._connect_timer.setSingleShot(True)
        self._connect_timer.timeout.connect(self._handle_connect_timeout)

        self._serial_poll_timer = QTimer()
        self._serial_poll_timer.timeout.connect(self._process_serial_queue)
        self._serial_poll_timer.start(30)

        self.setup_connections()
        self.refresh_ports()
        self.set_state(self.STATE_DISCONNECTED)

    def setup_connections(self):
        self.view.btn_actualizar.clicked.connect(self.refresh_ports)
        self.view.btn_conectar.clicked.connect(self.connect_serial)
        self.view.btn_desconectar.clicked.connect(self.disconnect_serial)
        self.view.btn_refrescar.clicked.connect(self.refresh_data)
        self.view.btn_iniciar.clicked.connect(self.start_test)
        self.view.github_button.clicked.connect(self.open_github)
        self.view.skyciv_link.clicked.connect(self.open_skyciv)

    def open_skyciv(self):
        if not SKYCIV_URL.strip():
            self.view.status_label.setText("URL de SkyCiv no configurada")
            return

        ok = QDesktopServices.openUrl(QUrl(SKYCIV_URL))

        if ok:
            self.view.status_label.setText("Abriendo SkyCiv...")
        else:
            self.view.status_label.setText("No se pudo abrir SkyCiv")

    def open_github(self):
        if not GITHUB_URL.strip():
            self.view.status_label.setText("URL de GitHub no configurada")
            return

        ok = QDesktopServices.openUrl(QUrl(GITHUB_URL))

        if ok:
            self.view.status_label.setText("Abriendo GitHub...")
        else:
            self.view.status_label.setText("No se pudo abrir GitHub")

    # -----------------------------------------
    # ESTADOS UI
    # -----------------------------------------
    def set_state(self, state):
        self.current_state = state

        style = STATUS_BAR_STYLES.get(
            state,
            {"color": "#808080", "percent": 0}
        )

        self.view.status_label.setText(state)
        self.view.card_status.set_value(state)
        self.view.card_status.set_percent(style["percent"])
        self.view.card_status.set_progress_color(style["color"])

        self.update_ui_state()

    def update_ui_state(self):
        state = self.current_state
        connected = self.serial_manager.is_connected()
        connecting = self._waiting_init_serial

        self.view.puerto_combo.setEnabled(not connected and not connecting)
        self.view.btn_actualizar.setEnabled(not connected and not connecting)
        self.view.btn_conectar.setEnabled(not connected and not connecting)

        if state == self.STATE_DISCONNECTED:
            self.view.btn_desconectar.setEnabled(False)
            self.view.btn_iniciar.setEnabled(False)
            self.view.btn_refrescar.setEnabled(False)

        elif state == self.STATE_CONNECTED:
            self.view.btn_desconectar.setEnabled(True)
            self.view.btn_iniciar.setEnabled(True)
            self.view.btn_refrescar.setEnabled(True)

        elif state == self.STATE_RUNNING:
            self.view.btn_desconectar.setEnabled(False)
            self.view.btn_iniciar.setEnabled(False)
            self.view.btn_refrescar.setEnabled(False)

    # -----------------------------------------
    # PUERTOS
    # -----------------------------------------
    def refresh_ports(self):
        puerto_actual = self.view.puerto_combo.currentText()

        self.view.puerto_combo.clear()
        ports = serial.tools.list_ports.comports()

        for port in ports:
            self.view.puerto_combo.addItem(port.device)

        if puerto_actual:
            index = self.view.puerto_combo.findText(puerto_actual)
            if index >= 0:
                self.view.puerto_combo.setCurrentIndex(index)

        if not ports:
            self.view.status_label.setText("Sin puertos disponibles")
            print("No se encontraron puertos serie")
        else:
            self.view.status_label.setText("Puertos actualizados")
            print("Puertos serie actualizados")

    # -----------------------------------------
    # CONEXIÓN
    # -----------------------------------------
    def connect_serial(self):
        print("[UI] Botón: Conectar")

        if self.serial_manager.is_connected():
            self.set_state(self.STATE_CONNECTED)
            self.view.status_label.setText("Ya conectado")
            return

        port = self.view.puerto_combo.currentText().strip()
        if not port:
            self.set_state(self.STATE_DISCONNECTED)
            self.view.status_label.setText("Seleccione un puerto")
            print("No hay puerto seleccionado")
            return

        success = self.serial_manager.connect(port)

        if not success:
            self.set_state(self.STATE_DISCONNECTED)
            self.view.status_label.setText(f"Error al conectar a {port}")
            print(f"No se pudo conectar a {port}")
            return

        print(f'Puerto abierto en {port}, esperando "{MSJ_INIT_COM}"...')
        self.serial_queue.clear()
        self.serial_manager.start_reading()

        self._connect_port_name = port
        self._waiting_init_serial = True
        self.view.status_label.setText(f"Conectado a {port}, esperando banco...")
        self.update_ui_state()

        self._connect_timer.start(2500)

    def _handle_connect_timeout(self):
        if not self._waiting_init_serial:
            return

        port = self._connect_port_name
        print(f'[HANDSHAKE] Timeout esperando "{MSJ_INIT_COM}" en {port}')

        self._waiting_init_serial = False
        self.serial_manager.disconnect()
        self.reset_connection_flags()
        self.reset_outputs()
        self.set_state(self.STATE_DISCONNECTED)

        QMessageBox.warning(
            self.view,
            "Banco no detectado",
            f"Conectado al {port}, pero no al banco.\n\n"
            f"No llegó el mensaje '{MSJ_INIT_COM}'."
        )

    def disconnect_serial(self):
        print("[UI] Botón: Desconectar")

        if not self.serial_manager.is_connected():
            self.set_state(self.STATE_DISCONNECTED)
            self.view.status_label.setText("No hay conexión activa")
            return

        self._connect_timer.stop()
        self._waiting_init_serial = False

        self.serial_manager.disconnect()
        print("Desconectado del puerto serie")

        self.reset_connection_flags()
        self.reset_outputs()
        self.set_state(self.STATE_DISCONNECTED)

    def reset_connection_flags(self):
        self._json_buffer.clear()
        self._receiving_json = False
        self._pending_start = False
        self._waiting_manual_response = False
        self._auto_refresh_done = False
        self._connect_port_name = ""

    def _process_serial_queue(self):
        while True:
            try:
                line = self.serial_queue.get_nowait()
                self.handle_serial_data(line)
            except Empty:
                break

    # -----------------------------------------
    # ALERTAS
    # -----------------------------------------
    def show_running_alert(self):
        QMessageBox.warning(
            self.view,
            "Ensayo en ejecución",
            "Espere que termine la ejecución del ensayo."
        )

    # -----------------------------------------
    # REFRESH
    # -----------------------------------------
    def refresh_data(self):
        print("[UI] Botón: Refrescar")

        if not self.serial_manager.is_connected():
            print("[FLOW] Refresh cancelado: serial desconectado")
            self.set_state(self.STATE_DISCONNECTED)
            return

        if self._waiting_init_serial:
            self.view.status_label.setText("Esperando banco...")
            return

        print("[FLOW] Refresh -> request_status()")
        self._waiting_manual_response = True
        self.request_status()

    # -----------------------------------------
    # INICIAR ENSAYO
    # -----------------------------------------
    def start_test(self):
        print("[UI] Botón: Iniciar")

        if not self.serial_manager.is_connected():
            print("[FLOW] Start cancelado: serial desconectado")
            self.set_state(self.STATE_DISCONNECTED)
            return

        if self._waiting_init_serial:
            self.view.status_label.setText("Esperando banco...")
            return

        print("[FLOW] Start -> pending_start=True -> request_status()")
        self._pending_start = True
        self._auto_refresh_done = False
        self._waiting_manual_response = True
        self.request_status()

    # -----------------------------------------
    # ENVÍO DE COMANDOS
    # -----------------------------------------
    def request_status(self):
        command = json.dumps({"info": "status"})
        ok = self.serial_manager.send_command(command)

        if ok:
            self.view.status_label.setText("Consultando estado...")
        else:
            self.view.status_label.setText("Error enviando status")

    def request_all_params(self):
        command = json.dumps({"info": "all-params"})
        ok = self.serial_manager.send_command(command)

        if ok:
            self.view.status_label.setText("Solicitando parámetros...")
        else:
            self.view.status_label.setText("Error enviando parámetros")

    def send_distance(self, distance_mm):
        command = json.dumps({"distance": int(distance_mm)})
        return self.serial_manager.send_command(command)

    def send_force(self, force_g):
        command = json.dumps({"force": int(force_g)})
        return self.serial_manager.send_command(command)

    def send_start_command(self):
        command = json.dumps({"cmd": "start"})
        return self.serial_manager.send_command(command)

    # -----------------------------------------
    # RECEPCIÓN
    # -----------------------------------------
    def handle_serial_data(self, line):
        line = line.strip()
        if not line:
            return

        print(f"Recibido: {line}")

        if line == MSJ_INIT_COM:
            self._handle_init_serial()
            return

        if self._waiting_init_serial and not line.startswith("{"):
            print(f"[HANDSHAKE] Ignorado durante espera: {line}")
            return

        self._json_buffer.append(line)
        self._try_parse_json_buffer()

    def _handle_init_serial(self):
        print(f'[HANDSHAKE] Recibido "{MSJ_INIT_COM}"')

        self._connect_timer.stop()
        self._waiting_init_serial = False

        if self.serial_manager.is_connected():
            self.set_state(self.STATE_CONNECTED)
            self.view.status_label.setText(f"Conectado a {self._connect_port_name}")
        else:
            self.set_state(self.STATE_DISCONNECTED)

    def _try_parse_json_buffer(self):
        raw = "".join(self._json_buffer)

        objects = []
        current = []
        depth = 0
        in_json = False

        for ch in raw:
            if ch == "{":
                depth += 1
                in_json = True

            if in_json:
                current.append(ch)

            if ch == "}":
                depth -= 1

                if in_json and depth == 0:
                    obj_text = "".join(current).strip()
                    if obj_text:
                        objects.append(obj_text)
                    current = []
                    in_json = False

        remainder = "".join(current).strip()
        self._json_buffer = [remainder] if remainder else []

        for obj_text in objects:
            try:
                data = json.loads(obj_text)
            except json.JSONDecodeError:
                print(f"[JSON] Error parseando: {obj_text}")
                continue

            self.process_json_data(data)

    # -----------------------------------------
    # PROCESAMIENTO JSON
    # -----------------------------------------
    def process_json_data(self, data):
        print(f"[JSON] process_json_data -> {data}")

        info = data.get("info")

        if info == "status":
            print("[JSON] Detectado info=status")
            self.process_status_response(data)
            return

        if info == "all-params":
            print("[JSON] Detectado info=all-params")
            self.process_all_params_response(data)
            return

        if "st_test" in data:
            print(f"[JSON] Detectado st_test suelto -> {data.get('st_test')}")

            try:
                st_test = int(data.get("st_test", 0))
            except Exception:
                st_test = 0

            if st_test == 0:
                if self.serial_manager.is_connected():
                    self.set_state(self.STATE_CONNECTED)
                    self.view.status_label.setText("Ensayo finalizado")

                if not self._waiting_manual_response and not self._auto_refresh_done:
                    print("[FLOW] Auto refresh por fin de ensayo")
                    self._auto_refresh_done = True
                    self.request_status()
            else:
                self.set_state(self.STATE_RUNNING)
                self.view.status_label.setText(f"Ensayo activo ({st_test})")

            return

        if data.get("result") == "ok":
            print("[JSON] Detectado result=ok")
            self.view.status_label.setText("Comando OK")
            return

        if data.get("result") == "ack":
            print("[JSON] Detectado result=ack")

            if data.get("cmd") == "start":
                self.set_state(self.STATE_RUNNING)
                self.view.status_label.setText("Ensayo iniciado")
            else:
                self.view.status_label.setText("Comando ACK")

            return

        print("[JSON] Mensaje no manejado")

    def process_status_response(self, data):
        print(f"[FLOW] process_status_response entrada -> {data}")

        status = data.get("status", 0)

        try:
            status = int(status)
        except (TypeError, ValueError):
            print("[FLOW] status inválido")
            self.view.status_label.setText("Estado inválido")
            self._pending_start = False
            self._waiting_manual_response = False
            return

        print(f"[FLOW] status parseado = {status}, pending_start = {self._pending_start}")

        if status == 0:
            if self.serial_manager.is_connected():
                self.set_state(self.STATE_CONNECTED)
        else:
            self.set_state(self.STATE_RUNNING)

        # Caso: se apretó INICIAR pero el ensayo ya estaba corriendo
        if self._pending_start:
            self._pending_start = False
            self._waiting_manual_response = False

            if status == 0:
                print("[FLOW] pending_start=True y status=0 -> start_sequence()")
                self.start_sequence()
            else:
                print("[FLOW] pending_start=True pero status!=0 -> alerta de ensayo en ejecución")
                self.set_state(self.STATE_RUNNING)
                self.view.status_label.setText("Ensayo en ejecución")
                self.show_running_alert()
            return

        # Caso: se apretó REFRESCAR y el ensayo está corriendo
        if self._waiting_manual_response:
            self._waiting_manual_response = False

            if status != 0:
                print("[FLOW] Refresh manual con status!=0 -> alerta de ensayo en ejecución")
                self.set_state(self.STATE_RUNNING)
                self.view.status_label.setText("Ensayo en ejecución")
                self.show_running_alert()
                return

        if status == 0:
            print("[FLOW] Refresh normal y status=0 -> request_all_params()")
            self.view.status_label.setText("Ensayo apagado, leyendo parámetros...")
            self.request_all_params()
        else:
            print("[FLOW] status!=0 -> ensayo activo")
            self.set_state(self.STATE_RUNNING)
            self.view.status_label.setText(f"Ensayo activo ({status})")

    def process_all_params_response(self, data):
        print(f"[FLOW] process_all_params_response -> {data}")

        self.update_measurements(data)
        self.update_inputs(data)

        print("[FLOW] UI actualizada con all-params")
        self._waiting_manual_response = False

        if self.serial_manager.is_connected():
            self.set_state(self.STATE_CONNECTED)

        self.view.status_label.setText("Parámetros actualizados")

    # -----------------------------------------
    # SECUENCIA DE INICIO
    # -----------------------------------------
    def start_sequence(self):
        distance_text = self.view.distance_input.text().strip()
        force_text = self.view.load_combo.currentText().strip()

        try:
            distance_mm = int(distance_text)
        except ValueError:
            self.view.status_label.setText("Distancia inválida")
            return

        try:
            force_g = int(force_text)
        except ValueError:
            self.view.status_label.setText("Carga inválida")
            return

        print(f"[FLOW] start_sequence -> distance={distance_mm} mm, force={force_g} g")

        ok_distance = self.send_distance(distance_mm)
        ok_force = self.send_force(force_g)
        ok_start = self.send_start_command()

        if ok_distance and ok_force and ok_start:
            self.view.status_label.setText("Comando de inicio enviado")
        else:
            self.view.status_label.setText("Error iniciando ensayo")

    # -----------------------------------------
    # UI DATA
    # -----------------------------------------
    def update_measurements(self, data):
        reaction_one = self._to_float(data.get("reaction_one", 0))
        reaction_two = self._to_float(data.get("reaction_two", 0))
        flexion = self._to_float(data.get("flexion", 0))

        self.view.card_r1.set_value(f"{reaction_one:.1f}")
        self.view.card_r1.set_gauge_value(reaction_one)

        self.view.card_r2.set_value(f"{reaction_two:.1f}")
        self.view.card_r2.set_gauge_value(reaction_two)

        if getattr(self.view, "card_flex", None) is not None:
            self.view.card_flex.set_value(f"{flexion:.2f}")
            self.view.card_flex.set_gauge_value(flexion)

    def update_inputs(self, data):
        distance = data.get("distance")
        force = data.get("force")

        if distance is not None:
            self.view.distance_input.setText(str(distance))

        if force is not None:
            force_text = str(force)
            idx = self.view.load_combo.findText(force_text)
            if idx >= 0:
                self.view.load_combo.setCurrentIndex(idx)
            else:
                self.view.load_combo.addItem(force_text)
                self.view.load_combo.setCurrentText(force_text)

    def reset_outputs(self):
        self.view.card_r1.set_value("0.0")
        self.view.card_r1.set_gauge_value(0)

        self.view.card_r2.set_value("0.0")
        self.view.card_r2.set_gauge_value(0)

        if getattr(self.view, "card_flex", None) is not None:
            self.view.card_flex.set_value("0.0")
            self.view.card_flex.set_gauge_value(0)

    def _to_float(self, value):
        try:
            return float(value)
        except (TypeError, ValueError):
            return 0.0