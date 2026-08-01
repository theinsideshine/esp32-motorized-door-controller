from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QFrame, QGridLayout, QHBoxLayout,
    QLabel, QLineEdit, QMainWindow, QPushButton, QScrollArea, QTabWidget,
    QVBoxLayout, QWidget,
)

from ui.door_position_widget import DoorPositionWidget


STATE_LABELS = {
    "UNKNOWN": "SIN CONFIRMAR",
    "DEV_BOOT": "BOOT", "DEV_CENTERING": "CENTERING", "DEV_READY": "READY",
    "DEV_OPENING_FWD": "CICLO FWD ACTIVO", "DEV_OPENING_REW": "CICLO REW ACTIVO",
    "DEV_DIAGNOSTIC_POSITIONING": "POSICIONAMIENTO DE DIAGNÓSTICO",
    "DEV_MANUAL_MOVING": "MOVIMIENTO MANUAL", "DEV_STOPPED": "STOPPED", "DEV_DANGER": "DANGER",
}


def panel():
    frame = QFrame(); frame.setObjectName("panel"); return frame


class ValueCard(QFrame):
    def __init__(self, title, accent=False):
        super().__init__(); self.setObjectName("valueCard")
        box = QVBoxLayout(self); box.setContentsMargins(16, 12, 16, 12); box.setSpacing(4)
        caption = QLabel(title.upper()); caption.setObjectName("cardCaption")
        self.value = QLabel("—"); self.value.setObjectName("accentValue" if accent else "cardValue"); self.value.setWordWrap(True)
        box.addWidget(caption); box.addWidget(self.value)


class MainWindow(QMainWindow):
    def __init__(self, model):
        super().__init__(); self.model = model
        self.setWindowTitle("Door Controller · Modo simulado")
        self.resize(1400, 900); self.setMinimumSize(1080, 720); self._apply_style()
        central = QWidget(); self.setCentralWidget(central)
        root = QVBoxLayout(central); root.setContentsMargins(20, 16, 20, 18); root.setSpacing(14)
        root.addWidget(self._build_header())
        self.tabs = QTabWidget()
        self.tabs.addTab(self._scroll(self._build_operation()), "  OPERACIÓN  ")
        self.tabs.addTab(self._scroll(self._build_diagnostics()), "  DIAGNÓSTICO Y PID  ")
        self.tabs.addTab(self._scroll(self._build_configuration()), "  CONFIGURACIÓN GENERAL  ")
        root.addWidget(self.tabs, 1)

    def _build_header(self):
        frame = QFrame(); frame.setObjectName("header")
        row = QHBoxLayout(frame); row.setContentsMargins(18, 12, 18, 12)
        titles = QVBoxLayout(); title = QLabel("CONTROL DE PUERTA MOTORIZADA"); title.setObjectName("appTitle")
        subtitle = QLabel("Operación por ciclos automáticos"); subtitle.setObjectName("muted")
        titles.addWidget(title); titles.addWidget(subtitle); row.addLayout(titles); row.addStretch()
        self.mode_combo = QComboBox(); self.mode_combo.addItems(["SIMULADO", "REAL"]); row.addWidget(self.mode_combo)
        self.mode_badge = QLabel("●  MODO SIMULADO"); self.mode_badge.setObjectName("simBadge"); row.addWidget(self.mode_badge); row.addSpacing(10)
        row.addWidget(QLabel("Puerto")); self.port_combo = QComboBox(); self.port_combo.addItems(["COM3 (simulado)", "COM4 (simulado)", "loop:// demo"]); row.addWidget(self.port_combo)
        self.refresh_ports_button = QPushButton("Actualizar"); row.addWidget(self.refresh_ports_button)
        self.connection_button = QPushButton("Conectar"); row.addWidget(self.connection_button)
        self.connection_status = QLabel("DESCONECTADO"); self.connection_status.setObjectName("connectionOff"); row.addWidget(self.connection_status)
        self.version_label = QLabel(self.model["identity"]["app_version"]); self.version_label.setObjectName("version"); row.addWidget(self.version_label)
        return frame

    def _build_operation(self):
        page = QWidget(); layout = QVBoxLayout(page); layout.setContentsMargins(4, 16, 4, 16); layout.setSpacing(14)
        summary = QGridLayout(); self.angle_card = ValueCard("Posición angular actual", True); self.state_card = ValueCard("Estado general")
        self.command_card = ValueCard("Último comando"); self.wait_card = ValueCard("open_wait_ms")
        for i, card in enumerate((self.angle_card, self.state_card, self.command_card, self.wait_card)): summary.addWidget(card, 0, i)
        layout.addLayout(summary)
        body = QHBoxLayout(); body.setSpacing(14)
        self.operation_position_widget = DoorPositionWidget()
        self.operation_position_widget.set_interaction_enabled(False)
        body.addWidget(self.operation_position_widget, 3)
        controls_panel = panel(); controls = QVBoxLayout(controls_panel); controls.setContentsMargins(20, 18, 20, 18)
        heading = QLabel("CONTROL PRINCIPAL"); heading.setObjectName("sectionTitle"); controls.addWidget(heading)
        hint = QLabel("FWD y REW solicitan un ciclo completo. La animación es sólo una representación local del comando activo."); hint.setObjectName("muted"); hint.setWordWrap(True); controls.addWidget(hint)
        self.fwd_button = QPushButton("FWD"); self.fwd_button.setObjectName("actionButton")
        self.rew_button = QPushButton("REW"); self.rew_button.setObjectName("actionButton")
        action_row = QHBoxLayout(); action_row.addWidget(self.fwd_button); action_row.addWidget(self.rew_button); controls.addLayout(action_row)
        self.stop_button = QPushButton("■  STOP"); self.stop_button.setObjectName("stopButton"); controls.addWidget(self.stop_button)
        controls.addStretch(); self.danger_label = QLabel(); self.danger_label.setObjectName("dangerIndicator"); self.danger_label.setWordWrap(True); controls.addWidget(self.danger_label)
        body.addWidget(controls_panel, 2); layout.addLayout(body)
        self.result_frame = QFrame(); self.result_frame.setObjectName("resultFrame"); result = QHBoxLayout(self.result_frame)
        caption = QLabel("ÚLTIMO RESULTADO / ERROR"); caption.setObjectName("cardCaption"); result.addWidget(caption)
        self.result_label = QLabel("—"); self.result_label.setObjectName("resultText"); self.result_label.setWordWrap(True); result.addWidget(self.result_label, 1); layout.addWidget(self.result_frame)
        return page

    def _build_diagnostics(self):
        page = QWidget(); layout = QVBoxLayout(page); layout.setContentsMargins(4, 16, 4, 16); layout.setSpacing(14)
        notice = QLabel("DIAGNÓSTICO · POS_1 y POS_3 abiertas · POS_2 centro/reposo · sensor absoluto 0°–360°"); notice.setObjectName("notice"); notice.setWordWrap(True); layout.addWidget(notice)
        top = QHBoxLayout(); top.setSpacing(14)
        self.position_widget = DoorPositionWidget(); top.addWidget(self.position_widget, 3)
        positions_panel = panel(); positions = QVBoxLayout(positions_panel); positions.setContentsMargins(18, 16, 18, 16)
        title = QLabel("POSICIONES Y MOVIMIENTO"); title.setObjectName("sectionTitle"); positions.addWidget(title)
        self.position_value_labels = {}
        for key, text in (("pos1_deg", "POS_1 · abierta"), ("pos2_deg", "POS_2 · centro/reposo"), ("pos3_deg", "POS_3 · abierta")):
            label = QLabel(); label.setObjectName("positionReadout"); self.position_value_labels[key] = label; positions.addWidget(label)
        self.pos_buttons = {}
        for pos in ("POS_1", "POS_2", "POS_3"):
            button = QPushButton(f"Ir a {pos}"); button.setObjectName("diagButton"); self.pos_buttons[pos] = button; positions.addWidget(button)
        self.diag_stop_button = QPushButton("■  STOP"); self.diag_stop_button.setObjectName("stopButton"); positions.addWidget(self.diag_stop_button); positions.addStretch()
        recovery = QLabel("Después de STOP: Ir a POS_2 recupera el estado READY."); recovery.setObjectName("recovery"); recovery.setWordWrap(True); positions.addWidget(recovery); top.addWidget(positions_panel, 2); layout.addLayout(top)

        metrics = QGridLayout(); self.metric_cards = {}
        for index, key in enumerate(("travel", "setpoint", "error", "abs_error", "arrival_tol", "pwm_cmd")):
            card = ValueCard(key); self.metric_cards[key] = card; metrics.addWidget(card, index//3, index%3)
        layout.addLayout(metrics)
        pid_panel = panel(); pid_layout = QVBoxLayout(pid_panel); pid_layout.setContentsMargins(18, 16, 18, 16)
        pid_title = QLabel("PID Y PARÁMETROS DE MOVIMIENTO · MODELO SIMULADO"); pid_title.setObjectName("sectionTitle"); pid_layout.addWidget(pid_title)
        pid_grid = QGridLayout(); self.pid_inputs = {}
        for index, key in enumerate(self.model["pid"]):
            field = QLineEdit(); self.pid_inputs[key] = field
            label = QLabel(key); label.setObjectName("fieldLabel"); pid_grid.addWidget(label, (index//4)*2, index%4); pid_grid.addWidget(field, (index//4)*2+1, index%4)
        pid_layout.addLayout(pid_grid); self.apply_pid_button = QPushButton("Aplicar PID al modelo simulado"); self.apply_pid_button.setObjectName("secondaryButton"); pid_layout.addWidget(self.apply_pid_button, alignment=Qt.AlignRight)
        layout.addWidget(pid_panel)
        return page

    def _build_configuration(self):
        page = QWidget(); layout = QVBoxLayout(page); layout.setContentsMargins(4, 16, 4, 16); layout.setSpacing(14)
        notice = QLabel("CONFIGURACIÓN LOCAL · Leer, Aplicar y Restaurar operan sólo sobre el modelo JSON cargado en memoria."); notice.setObjectName("notice"); notice.setWordWrap(True); layout.addWidget(notice)
        config_panel = panel(); form = QFormLayout(config_panel); form.setContentsMargins(24, 22, 24, 22); form.setHorizontalSpacing(28); form.setVerticalSpacing(13)
        self.config_inputs = {}
        for key in ("open_wait_ms", "danger_time_ms", "led_blink_ms", "log_level", "st_mode"):
            field = QLineEdit(); self.config_inputs[key] = field; form.addRow(key, field)
        self.led_enabled_input = QCheckBox("LED habilitado"); self.config_inputs["led_enabled"] = self.led_enabled_input; form.addRow("led_enabled", self.led_enabled_input)
        motion_info = QLabel(); motion_info.setObjectName("readonlyValue"); self.motion_mode_info = motion_info; form.addRow("motion_mode (informativo)", motion_info)
        layout.addWidget(config_panel)
        actions = QHBoxLayout(); self.read_config_button = QPushButton("Leer"); self.apply_config_button = QPushButton("Aplicar"); self.apply_config_button.setObjectName("actionButton")
        self.restore_config_button = QPushButton("Restaurar"); self.factory_reset_button = QPushButton("Factory reset simulado"); self.factory_reset_button.setObjectName("dangerOutline")
        for button in (self.read_config_button, self.apply_config_button, self.restore_config_button): actions.addWidget(button)
        actions.addStretch(); actions.addWidget(self.factory_reset_button); layout.addLayout(actions)
        self.config_feedback = QLabel(); self.config_feedback.setObjectName("feedback"); self.config_feedback.setWordWrap(True); layout.addWidget(self.config_feedback); layout.addStretch()
        return page

    def render(self, model, connected, danger_flash=False):
        state, telemetry, positions, config, sim = model["state"], model["telemetry"], model["positions"], model["configuration"], model["simulation"]
        device_state = state["device_state"]
        auto_tolerance = float(model["pid"]["auto_tolerance_deg"])
        centered = abs(self._angular_delta(telemetry["current_angle_deg"], positions["pos2_deg"])) <= auto_tolerance
        active_states = {"FWD_ACTIVE", "REW_ACTIVE", "GO_POS1_ACTIVE", "GO_POS2_ACTIVE", "GO_POS3_ACTIVE"}
        movement_active = sim["presentation_state"] in active_states
        confirmed_ready = device_state == ("READY" if self.mode_combo.currentText() == "REAL" else "DEV_READY")
        position_allows_cycle = True if self.mode_combo.currentText() == "REAL" else centered
        can_start_product_cycle = connected and confirmed_ready and position_allows_cycle and not movement_active
        self.fwd_button.setEnabled(can_start_product_cycle); self.rew_button.setEnabled(can_start_product_cycle)
        self.stop_button.setEnabled(connected and movement_active); self.diag_stop_button.setEnabled(connected and movement_active)
        for button in self.pos_buttons.values(): button.setEnabled(connected and device_state not in ("DEV_DANGER", "DANGER"))
        self.angle_card.value.setText(f'{telemetry["current_angle_deg"]:.2f}°'); self.state_card.value.setText(STATE_LABELS.get(device_state, device_state))
        self.command_card.value.setText(state["last_command"]); self.wait_card.value.setText(f'{config["open_wait_ms"]} ms')
        self.operation_position_widget.set_positions(positions["pos1_deg"], positions["pos2_deg"], positions["pos3_deg"])
        operation_setpoint = telemetry["setpoint"] if telemetry.get("setpoint") is not None else telemetry["current_angle_deg"]
        self.operation_position_widget.set_telemetry(telemetry["current_angle_deg"], operation_setpoint,
                                                     telemetry.get("rotation_direction", "NONE"), telemetry["arrival_tol"])
        local_presentation = sim["presentation_state"]
        self.operation_position_widget.set_presentation_state(local_presentation)
        confirmed = state["danger_confirmed"] or device_state == "DEV_DANGER"
        self.danger_label.setText("DANGER CONFIRMADO" if confirmed else "DANGER · sin confirmar")
        self.danger_label.setProperty("confirmed", confirmed and danger_flash); self._repolish(self.danger_label)
        result = state["last_result"]
        if result.get("reason"):
            self.result_label.setText(f'ERROR · reason = {result["reason"]}'); self.result_frame.setProperty("error", True)
        else:
            self.result_label.setText(result.get("message", "Sin errores")); self.result_frame.setProperty("error", False)
        self._repolish(self.result_frame)
        self.position_widget.set_positions(positions["pos1_deg"], positions["pos2_deg"], positions["pos3_deg"])
        self.position_widget.set_telemetry(telemetry["travel"], telemetry["setpoint"], telemetry["rotation_direction"], telemetry["arrival_tol"])
        self.position_widget.set_presentation_state(local_presentation)
        for key in self.position_value_labels: self.position_value_labels[key].setText(f'{key.upper()}  ·  {positions[key]:.2f}°')
        for key, card in self.metric_cards.items(): card.value.setText(f'{telemetry[key]:.2f}' + ("°" if key != "pwm_cmd" else ""))
        self.motion_mode_info.setText(str(model["pid"]["motion_mode"]))

    def populate_pid(self, pid):
        for key, value in pid.items(): self.pid_inputs[key].setText(str(value))

    def populate_configuration(self, config):
        for key, widget in self.config_inputs.items():
            widget.setChecked(bool(config[key])) if key == "led_enabled" else widget.setText(str(config[key]))

    def set_connected(self, connected):
        self.connection_button.setText("Desconectar" if connected else "Conectar"); self.connection_status.setText("CONECTADO · SIM" if connected else "DESCONECTADO")
        self.connection_status.setObjectName("connectionOn" if connected else "connectionOff"); self._repolish(self.connection_status)
        self.port_combo.setEnabled(not connected); self.mode_combo.setEnabled(not connected); self.refresh_ports_button.setEnabled(not connected)

    def set_mode(self, real_mode):
        self.mode_badge.setText("●  MODO REAL" if real_mode else "●  MODO SIMULADO")
        self.mode_badge.setObjectName("realBadge" if real_mode else "simBadge")
        self._repolish(self.mode_badge)

    @staticmethod
    def _angular_delta(a, b): return (b-a+180.0)%360.0-180.0
    @staticmethod
    def _repolish(widget): widget.style().unpolish(widget); widget.style().polish(widget)
    @staticmethod
    def _scroll(widget):
        scroll = QScrollArea(); scroll.setWidgetResizable(True); scroll.setFrameShape(QFrame.NoFrame); scroll.setWidget(widget); return scroll

    def _apply_style(self):
        self.setStyleSheet("""
            QMainWindow, QWidget { background: #0d1119; color: #eef3f8; font-family: 'Segoe UI'; font-size: 13px; }
            QFrame#header, QFrame#panel { background: #121a26; border: 1px solid #263449; border-radius: 14px; }
            QLabel#appTitle { font-size: 21px; font-weight: 800; letter-spacing: 1px; } QLabel#muted, QLabel#version { color: #91a0b5; }
            QLabel#simBadge { background: #4b3510; color: #ffd66b; border: 1px solid #936c1f; border-radius: 9px; padding: 8px 12px; font-weight: 800; }
            QLabel#realBadge { background: #123b2b; color: #70e6aa; border: 1px solid #297653; border-radius: 9px; padding: 8px 12px; font-weight: 800; }
            QLabel#connectionOff { color: #9aa6b5; font-weight: 800; padding: 7px; } QLabel#connectionOn { color: #63e6a3; font-weight: 800; padding: 7px; }
            QComboBox, QLineEdit, QPushButton { background: #1b2636; border: 1px solid #3a4c64; border-radius: 9px; padding: 9px 12px; color: white; font-weight: 650; }
            QLineEdit:focus { border-color: #58a6e7; } QPushButton:hover { background: #26364c; border-color: #66a8ff; } QPushButton:disabled { color: #5c6878; background: #151b24; border-color: #252e3b; }
            QPushButton#actionButton { min-height: 48px; background: #174a72; border-color: #3b91d1; font-size: 15px; } QPushButton#secondaryButton { background: #173c52; border-color: #2c718f; }
            QPushButton#stopButton { min-height: 48px; background: #8e1723; border: 2px solid #ff5968; font-size: 17px; font-weight: 900; } QPushButton#stopButton:disabled { background: #37191e; border-color: #64313a; color: #8d6970; }
            QPushButton#dangerOutline { color: #ff8691; border-color: #93333d; } QPushButton#diagButton { min-height: 38px; }
            QTabWidget::pane { border: 0; } QTabBar::tab { background: #131b27; color: #91a0b5; padding: 12px 26px; margin-right: 4px; border-radius: 8px; font-weight: 800; } QTabBar::tab:selected { color: white; background: #23598f; }
            QFrame#valueCard { background: #172231; border: 1px solid #2b3b50; border-radius: 11px; min-height: 70px; } QLabel#cardCaption { color: #7f91a9; font-size: 11px; font-weight: 800; } QLabel#cardValue { font-size: 18px; font-weight: 800; } QLabel#accentValue { color: #6fc6ff; font-size: 27px; font-weight: 900; }
            QLabel#sectionTitle { color: #8ecaff; font-size: 14px; font-weight: 900; } QLabel#fieldLabel { color: #8b9bb0; font-size: 11px; } QLabel#positionReadout { background: #172231; border-radius: 7px; padding: 8px; font-weight: 700; }
            QFrame#resultFrame { background: #14261f; border: 1px solid #286447; border-radius: 11px; } QFrame#resultFrame[error='true'] { background: #351319; border: 1px solid #b93645; } QLabel#resultText { padding: 9px; font-weight: 700; }
            QLabel#dangerIndicator { background: #1a222e; color: #75859a; border-radius: 8px; padding: 10px; font-weight: 900; } QLabel#dangerIndicator[confirmed='true'] { background: #a51625; color: white; }
            QLabel#notice { background: #172b35; color: #9cdbea; border-left: 4px solid #48aabe; padding: 12px; font-weight: 700; } QLabel#recovery { background: #282517; color: #f3d878; border-radius: 8px; padding: 10px; font-weight: 700; }
            QLabel#readonlyValue { color: #8ecaff; font-weight: 800; } QLabel#feedback { background: #172231; border-radius: 8px; padding: 12px; color: #9cdbea; } QScrollArea { border: none; }
        """)
