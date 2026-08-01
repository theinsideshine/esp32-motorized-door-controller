import json
from copy import deepcopy
from queue import Empty

import serial.tools.list_ports
from PySide6.QtCore import QObject, QTimer

from core.serial_event_queue import SerialEventQueue
from core.serial_manager import SerialManager


class MainController(QObject):
    """Simulation plus real JSON transport; confirmed state only comes from JSON in real mode."""

    def __init__(self, view, model):
        super().__init__(view)
        self.view, self.model = view, model
        self.model["simulation"].setdefault("pending_command", None)
        self.serial_manager = SerialManager(); self.serial_queue = SerialEventQueue(); self.serial_manager.set_event_queue(self.serial_queue)
        self._factory_defaults = deepcopy(model); self._applied_config = deepcopy(model["configuration"])
        self._danger_flash = False; self._diagnostic_target = None; self._json_buffer = ""; self._awaiting_version = False
        self._cycle_timer = self._single_timer(self._finish_simulated_cycle)
        self._diagnostic_timer = self._single_timer(self._finish_simulated_diagnostic)
        self._poll_timer = QTimer(self); self._poll_timer.timeout.connect(self._process_serial_queue); self._poll_timer.start(30)
        self._blink_timer = QTimer(self); self._blink_timer.timeout.connect(self._blink_danger); self._blink_timer.start(450)
        self._connect_signals(); self.view.populate_pid(model["pid"]); self.view.populate_configuration(model["configuration"])
        self.view.set_mode(False); self.view.set_connected(False); self.refresh()

    def _single_timer(self, callback):
        timer = QTimer(self); timer.setSingleShot(True); timer.timeout.connect(callback); return timer

    def _connect_signals(self):
        self.view.mode_combo.currentTextChanged.connect(self.change_mode); self.view.refresh_ports_button.clicked.connect(self.refresh_ports)
        self.view.tabs.currentChanged.connect(lambda index: self.refresh())
        self.view.connection_button.clicked.connect(self.toggle_connection)
        self.view.fwd_button.clicked.connect(lambda: self.send_cycle("fwd")); self.view.rew_button.clicked.connect(lambda: self.send_cycle("rew"))
        self.view.stop_button.clicked.connect(self.stop); self.view.diag_stop_button.clicked.connect(self.stop)
        for position, button in self.view.pos_buttons.items(): button.clicked.connect(lambda checked=False, p=position: self.go_position(p))
        self.view.position_widget.positionSelected.connect(lambda key: self.go_position({"pos1_deg":"POS_1", "pos2_deg":"POS_2", "pos3_deg":"POS_3"}[key]))
        self.view.read_config_button.clicked.connect(self.read_configuration); self.view.apply_config_button.clicked.connect(self.apply_configuration)
        self.view.restore_config_button.clicked.connect(self.restore_configuration); self.view.factory_reset_button.clicked.connect(self.factory_reset)
        self.view.apply_pid_button.clicked.connect(self.apply_pid)

    @property
    def real_mode(self): return self.view.mode_combo.currentText() == "REAL"

    def change_mode(self):
        if self.serial_manager.is_connected(): self.disconnect_real()
        self.model["simulation"].update(connected=False, presentation_state="IDLE", pending_command=None)
        self.view.set_mode(self.real_mode); self.view.set_connected(False)
        if self.real_mode:
            self.model["state"]["device_state"] = "UNKNOWN"
            self.refresh_ports()
        else:
            self.view.port_combo.clear(); self.view.port_combo.addItems(["COM3 (simulado)", "COM4 (simulado)", "loop:// demo"])
        self.refresh()

    def refresh_ports(self):
        if not self.real_mode: return
        current = self.view.port_combo.currentText(); ports = [p.device for p in serial.tools.list_ports.comports()]
        self.view.port_combo.clear(); self.view.port_combo.addItems(ports)
        if current in ports: self.view.port_combo.setCurrentText(current)
        self.model["state"]["last_result"] = ({"result":"ok", "message":f"{len(ports)} puerto(s) detectado(s)"} if ports else {"reason":"no_serial_ports"})
        self.refresh()

    def toggle_connection(self):
        if self.real_mode:
            self.disconnect_real() if self.serial_manager.is_connected() else self.connect_real()
        else: self.toggle_simulated_connection()

    def connect_real(self):
        port = self.view.port_combo.currentText().strip()
        if not port:
            self.model["state"]["last_result"] = {"reason":"serial_port_not_selected"}; self.refresh(); return
        if not self.serial_manager.connect(port):
            self.model["state"]["last_result"] = {"reason":"serial_connect_failed"}; self.refresh(); return
        self.serial_queue.clear(); self._json_buffer = ""; self.serial_manager.start_reading()
        self.model["state"]["device_state"] = "UNKNOWN"
        self.model["simulation"].update(connected=True, presentation_state="IDLE", pending_command=None); self.view.set_connected(True)
        self.model["state"]["last_result"] = {"result":"ok", "message":f"Puerto {port} abierto · solicitando versión"}
        self._awaiting_version = True; self._send_json({"info":"version"}); self.refresh()

    def disconnect_real(self):
        self.serial_manager.disconnect(); self.serial_queue.clear(); self._awaiting_version = False
        self.model["simulation"].update(connected=False, presentation_state="IDLE", pending_command=None)
        self.model["state"]["last_result"] = {"result":"ok", "message":"Puerto serie desconectado"}
        self.view.set_connected(False); self.refresh()

    def toggle_simulated_connection(self):
        connected = not self.model["simulation"]["connected"]; self._cycle_timer.stop(); self._diagnostic_timer.stop()
        self.model["simulation"].update(connected=connected, presentation_state="IDLE", pending_command=None)
        state, telemetry, positions = self.model["state"], self.model["telemetry"], self.model["positions"]
        if connected:
            state.update(device_state="DEV_READY", selected_command=None, last_result={"result":"ok", "message":"Conexión simulada establecida · READY"})
            telemetry.update(current_angle_deg=positions["pos2_deg"], travel=positions["pos2_deg"], setpoint=positions["pos2_deg"], error=0.0, abs_error=0.0, pwm_cmd=0.0, rotation_direction="NONE")
        else: state.update(selected_command=None, last_result={"reason":"device_not_ready"})
        self.view.set_connected(connected); self.refresh()

    def send_cycle(self, command):
        if not self._ready_centered():
            self.model["state"]["last_result"] = {"reason":"device_not_ready"}; self.model["simulation"]["presentation_state"] = "ERROR"; self.refresh(); return
        if self.real_mode:
            if self._send_json({"cmd":command}): self._mark_pending(command.upper())
        else:
            self._mark_requested(command.upper(), f"{command.upper()}_ACTIVE")
            self.model["state"]["last_result"] = {"result":"ack", "message":f"ACK simulado · ciclo {command.upper()} solicitado"}
            self._cycle_timer.start(int(self.model["simulation"]["cycle_duration_ms"]))
        self.refresh()

    def _mark_requested(self, command, presentation):
        self.model["state"].update(selected_command=command, last_command=command,
                                   last_result={"result":"requested", "message":f"Comando {command} solicitado · esperando respuesta"})
        self.model["simulation"]["presentation_state"] = presentation

    def _mark_pending(self, command):
        self.model["simulation"]["pending_command"] = command
        self.model["state"].update(last_command=command,
                                   last_result={"result":"requested", "message":f"Comando {command} solicitado · esperando ACK"})

    def _finish_simulated_cycle(self):
        state, telemetry, positions = self.model["state"], self.model["telemetry"], self.model["positions"]
        command = state["selected_command"] or state["last_command"]
        state.update(device_state="DEV_READY", selected_command=None, last_result={"result":"ok", "message":f"Finalización simulada · ciclo {command} completo"})
        telemetry.update(current_angle_deg=positions["pos2_deg"], travel=positions["pos2_deg"], setpoint=positions["pos2_deg"], error=0.0, abs_error=0.0, pwm_cmd=0.0, rotation_direction="NONE")
        self.model["simulation"]["presentation_state"] = "IDLE"; self.refresh()

    def stop(self):
        if not self.model["simulation"]["connected"]: return
        if self.real_mode:
            if self._send_json({"cmd":"stop"}): self._mark_pending("STOP")
        else:
            self._cycle_timer.stop(); self._diagnostic_timer.stop(); self._mark_requested("STOP", "STOPPED")
            self.model["state"].update(device_state="DEV_STOPPED", selected_command=None, last_result={"result":"ack", "message":"ACK simulado · STOP"})
            t = self.model["telemetry"]; t.update(setpoint=t["travel"], error=0.0, abs_error=0.0, pwm_cmd=0.0, rotation_direction="NONE")
        self.refresh()

    def go_position(self, position):
        if not self.model["simulation"]["connected"]: self.model["state"]["last_result"]={"reason":"device_not_ready"}; self.refresh(); return
        number = {"POS_1":1, "POS_2":2, "POS_3":3}[position]
        if self.real_mode:
            if self._send_json({"cmd":"go", "pos":number}): self._mark_pending(f"GO_{position}")
        else: self._start_simulated_diagnostic(position)
        self.refresh()

    def _start_simulated_diagnostic(self, position):
        key={"POS_1":"pos1_deg","POS_2":"pos2_deg","POS_3":"pos3_deg"}[position]; target=self.model["positions"][key]
        t=self.model["telemetry"]; delta=self._angular_delta(t["travel"],target)
        t.update(setpoint=target,error=delta,abs_error=abs(delta),pwm_cmd=float(self.model["pid"]["pwm_move"]),rotation_direction="CW" if delta>0 else "CCW")
        self.model["state"].update(device_state="DEV_DIAGNOSTIC_POSITIONING",selected_command=f"GO_{position}",last_command=f"GO_{position}",last_result={"result":"ack","message":f"ACK simulado · {position}"})
        self.model["simulation"]["presentation_state"] = f"GO_POS{position[-1]}_ACTIVE"
        self._diagnostic_target=(position,target); self.view.position_widget.set_selected_position(key); self._diagnostic_timer.start(850)

    def _finish_simulated_diagnostic(self):
        if not self._diagnostic_target:return
        position,target=self._diagnostic_target; self._diagnostic_target=None; final="DEV_READY" if position=="POS_2" else "DEV_STOPPED"
        self.model["telemetry"].update(current_angle_deg=target,travel=target,setpoint=target,error=0.0,abs_error=0.0,pwm_cmd=0.0,rotation_direction="NONE")
        self.model["state"].update(device_state=final,selected_command=None,last_result={"result":"ok","message":"Recuperación simulada · READY" if final=="DEV_READY" else f"{position} alcanzada"})
        self.model["simulation"]["presentation_state"]=f"IDLE_POS{position[-1]}"; self.refresh()

    def _send_json(self, payload):
        ok=self.serial_manager.send_command(json.dumps(payload,separators=(",",":")))
        if not ok:self.model["state"]["last_result"]={"reason":"serial_send_failed"}
        return ok

    def _process_serial_queue(self):
        while True:
            try:self._consume_serial_line(self.serial_queue.get_nowait())
            except Empty:break

    def _consume_serial_line(self, line):
        text=line.strip()
        if not text:return
        if not self._json_buffer and not text.startswith("{"):
            print(f"[FW LOG] {text}"); return
        self._json_buffer += text
        decoder=json.JSONDecoder()
        while self._json_buffer:
            try:data,end=decoder.raw_decode(self._json_buffer)
            except json.JSONDecodeError:return
            self._json_buffer=self._json_buffer[end:].lstrip()
            if isinstance(data,dict):self.process_json(data)

    def _process_runtime_message(self, data):
        event=data.get("event")
        if event=="motion-complete":
            self._handle_motion_complete(data); return True
        if event=="cycle-complete":
            self._handle_cycle_complete(data); return True
        if data.get("result")=="ack" and data.get("cmd"):
            self._handle_command_ack(data); return True
        if data.get("result")=="error":
            self._handle_command_error(data); return True
        return False

    def _handle_command_ack(self, data):
        command=str(data.get("cmd","")).lower(); sim=self.model["simulation"]; state=self.model["state"]
        if command in ("fwd","rew"):
            selected=command.upper(); state["selected_command"]=selected; sim["presentation_state"]=f"{selected}_ACTIVE"
        elif command=="go":
            position=int(data.get("pos",0)); state["selected_command"]=f"GO_POS_{position}"
            if position in (1,2,3):sim["presentation_state"]=f"GO_POS{position}_ACTIVE"
        elif command=="stop":
            state["selected_command"]=None; sim["presentation_state"]="STOPPED"
        sim["pending_command"]=None
        state["last_result"]={"result":"ack","message":f"ACK recibido · {command}"}

    def _handle_motion_complete(self, data):
        if data.get("result")!="ok" or data.get("reason")!="posicion_alcanzada":
            self._handle_command_error(data); return
        position=int(data.get("pos",0)); self._apply_terminal_values(data)
        self.model["state"].update(selected_command=None,last_result={"result":"ok","message":f"Movimiento POS_{position} completado por firmware"})
        self.model["simulation"].update(presentation_state=f"IDLE_POS{position}" if position in (1,2,3) else "IDLE",pending_command=None)

    def _handle_cycle_complete(self, data):
        if data.get("result")!="ok" or data.get("reason")!="posicion_alcanzada":
            self._handle_command_error(data); return
        command=str(data.get("cmd","")).upper(); self._apply_terminal_values(data)
        self.model["state"].update(selected_command=None,last_result={"result":"ok","message":f"Ciclo {command} completado por firmware"})
        self.model["simulation"].update(presentation_state="IDLE",pending_command=None)

    def _apply_terminal_values(self, data):
        telemetry=self.model["telemetry"]
        if data.get("final_deg") is not None:
            final=float(data["final_deg"]); telemetry["current_angle_deg"]=final; telemetry["travel"]=final
        if data.get("target_deg") is not None:telemetry["setpoint"]=float(data["target_deg"])
        telemetry.update(error=0.0,abs_error=0.0,pwm_cmd=0.0,rotation_direction="NONE")
        if data.get("device_state") is not None:self.model["state"]["device_state"]=str(data["device_state"])

    def _handle_command_error(self, data):
        state=self.model["state"]; state["selected_command"]=None
        if data.get("device_state") is not None:state["device_state"]=str(data["device_state"])
        state["last_result"]={"result":"error","reason":data.get("reason","firmware_error")}
        self.model["simulation"].update(presentation_state="ERROR",pending_command=None)

    def process_json(self, data):
        print(f"[FW JSON] {data}")
        if "app_version" in data:
            self.model["identity"]["app_version"]=str(data["app_version"]); self.view.version_label.setText(str(data["app_version"]))
        self._merge_firmware_values(data)
        if self._process_runtime_message(data):
            self.refresh(); return
        result=data.get("result")
        if result in ("ack","ok"):
            message=f"{result.upper()} recibido"; command=data.get("cmd") or self.model["state"]["selected_command"]
            self.model["state"]["last_result"]={"result":result,"message":f"{message}" + (f" · {command}" if command else "")}
            if result=="ok" and data.get("completed") is True:self.model["simulation"]["presentation_state"]="IDLE"; self.model["state"]["selected_command"]=None
        elif result=="error":
            self.model["state"]["last_result"]={"result":"error","reason":data.get("reason","firmware_error")}; self.model["simulation"]["presentation_state"]="ERROR"
        elif "reason" in data:
            self.model["state"]["last_result"]={"result":"error","reason":data["reason"]}; self.model["simulation"]["presentation_state"]="ERROR"
        if self._awaiting_version and ("app_version" in data or data.get("info")=="version" or result in ("ack","ok")):
            self._awaiting_version=False; self._send_json({"info":"all-params"})
        self.refresh()

    def _merge_firmware_values(self, data):
        source=data.get("params",data); mappings={"positions":self.model["positions"],"pid":self.model["pid"],"configuration":self.model["configuration"],"telemetry":self.model["telemetry"]}
        for target in mappings.values():
            for key in tuple(target):
                if key in source and source[key] is not None:
                    try:target[key]=type(target[key])(source[key]) if not isinstance(target[key],bool) else bool(source[key])
                    except (TypeError,ValueError):print(f"[FW JSON] Valor inválido ignorado: {key}={source[key]!r}")
        if "device_state" in source:
            self.model["state"]["device_state"]=str(source["device_state"])
            confirmed_state=str(source["device_state"])
            danger=confirmed_state in ("DEV_DANGER","DANGER"); self.model["state"]["danger_confirmed"]=danger
            if danger:self.model["simulation"]["presentation_state"]="DANGER"
            elif confirmed_state in ("DEV_OPENING_FWD","OPENING_FWD"):self.model["simulation"]["presentation_state"]="FWD_ACTIVE"
            elif confirmed_state in ("DEV_OPENING_REW","OPENING_REW"):self.model["simulation"]["presentation_state"]="REW_ACTIVE"
            elif confirmed_state in ("DEV_OPEN_WAIT","DEV_CLOSING_CENTER") and self.model["state"]["selected_command"] in ("FWD","REW"):
                self.model["simulation"]["presentation_state"]=f'{self.model["state"]["selected_command"]}_ACTIVE'
            elif confirmed_state=="DEV_READY":self.model["simulation"]["presentation_state"]="IDLE"; self.model["state"]["selected_command"]=None
        if "current_angle_deg" in source:self.model["telemetry"]["current_angle_deg"]=float(source["current_angle_deg"]); self.model["telemetry"]["travel"]=float(source["current_angle_deg"])
        if "current_deg" in source:self.model["telemetry"]["current_angle_deg"]=float(source["current_deg"]); self.model["telemetry"]["travel"]=float(source["current_deg"])
        if data.get("info")=="all-params" or "params" in data:
            if self.model["state"]["device_state"] in ("READY","STOPPED"):
                self.model["simulation"].update(presentation_state="IDLE",pending_command=None)
                self.model["state"]["selected_command"]=None
            self.view.populate_configuration(self.model["configuration"]); self.view.populate_pid(self.model["pid"])

    def read_configuration(self):
        if self.real_mode and self.serial_manager.is_connected():self._send_json({"info":"all-params"}); self.view.config_feedback.setText("Solicitud all-params enviada al firmware.")
        else:self.view.populate_configuration(self.model["configuration"]); self.view.config_feedback.setText("Lectura local completada.")

    def apply_configuration(self):
        try:values=self._read_config_fields()
        except ValueError as exc:self.view.config_feedback.setText(f"Error de validación · {exc}");return
        self.model["configuration"].update(values);self._applied_config=deepcopy(values);self.view.config_feedback.setText("Aplicado al modelo local; envío de configuración real aún no habilitado.");self.refresh()
    def restore_configuration(self):self.model["configuration"]=deepcopy(self._applied_config);self.view.populate_configuration(self.model["configuration"]);self.refresh()
    def factory_reset(self):
        self.model["configuration"]=deepcopy(self._factory_defaults["configuration"]);self.model["pid"]=deepcopy(self._factory_defaults["pid"]);self.view.populate_configuration(self.model["configuration"]);self.view.populate_pid(self.model["pid"]);self.view.config_feedback.setText("Factory reset local simulado.");self.refresh()
    def apply_pid(self):
        try:
            ints={"motion_mode","pwm_move","pid_pwm_max","pid_pwm_min_effective","control_period_us"};values={k:(int(f.text()) if k in ints else float(f.text())) for k,f in self.view.pid_inputs.items()}
            if values["control_period_us"]<=0 or values["pid_pwm_min_effective"]>values["pid_pwm_max"]:raise ValueError("rangos PID inconsistentes")
        except ValueError as exc:self.view.config_feedback.setText(f"Error PID · {exc}");return
        self.model["pid"].update(values);self.model["telemetry"]["arrival_tol"]=values["auto_tolerance_deg"];self.view.config_feedback.setText("PID actualizado sólo localmente.");self.refresh()
    def _read_config_fields(self):
        f=self.view.config_inputs;v={"open_wait_ms":int(f["open_wait_ms"].text()),"danger_time_ms":int(f["danger_time_ms"].text()),"led_enabled":f["led_enabled"].isChecked(),"led_blink_ms":int(f["led_blink_ms"].text()),"log_level":f["log_level"].text().strip().upper(),"st_mode":int(f["st_mode"].text())}
        if v["open_wait_ms"]<0 or v["danger_time_ms"]<=0 or v["led_blink_ms"]<=0:raise ValueError("tiempos inválidos")
        if v["log_level"] not in {"ERROR","WARN","INFO","DEBUG","TRACE"}:raise ValueError("log_level inválido")
        return v
    def refresh(self):self.view.render(self.model,self.model["simulation"]["connected"],self._danger_flash)
    def _ready_centered(self):
        t,p,s=self.model["telemetry"],self.model["positions"],self.model["state"]
        active_states={"FWD_ACTIVE","REW_ACTIVE","GO_POS1_ACTIVE","GO_POS2_ACTIVE","GO_POS3_ACTIVE"}
        movement_active=self.model["simulation"]["presentation_state"] in active_states
        if self.real_mode:
            return self.model["simulation"]["connected"] and s["device_state"]=="READY" and not movement_active
        return (self.model["simulation"]["connected"] and s["device_state"]=="DEV_READY"
                and abs(self._angular_delta(t["current_angle_deg"],p["pos2_deg"]))<=self.model["pid"]["auto_tolerance_deg"]
                and not movement_active)
    def _blink_danger(self):
        self._danger_flash=not self._danger_flash
        if self.model["state"]["danger_confirmed"]:self.refresh()
    @staticmethod
    def _angular_delta(start,end):return (end-start+180.0)%360.0-180.0
