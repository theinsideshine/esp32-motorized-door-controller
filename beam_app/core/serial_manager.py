import threading
import serial


class SerialManager:
    def __init__(self):
        self.serial = None
        self.thread = None
        self.running = False
        self.event_queue = None

    def set_event_queue(self, event_queue):
        self.event_queue = event_queue

    def connect(self, port, baudrate=115200):
        try:
            self.serial = serial.Serial(port, baudrate, timeout=1)
            return True
        except serial.SerialException as e:
            print(f"Error abriendo el puerto serie: {e}")
            self.serial = None
            return False

    def disconnect(self):
        self.running = False

        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=1)

        if self.serial and self.serial.is_open:
            self.serial.close()

        self.serial = None
        self.thread = None

    def is_connected(self):
        return self.serial is not None and self.serial.is_open

    def send_command(self, command):
        if self.serial and self.serial.is_open:
            try:
                self.serial.write((command + "\n").encode())
                print(f"Enviado: {command}")
                return True
            except Exception as e:
                print(f"Error enviando comando: {e}")
                return False
        return False

    def start_reading(self):
        def run():
            while self.running:
                try:
                    if self.serial and self.serial.is_open and self.serial.in_waiting:
                        line = self.serial.readline().decode(errors="ignore").strip()

                        if line and self.event_queue:
                            self.event_queue.put(line)

                except Exception as e:
                    print(f"Error leyendo datos: {e}")

        self.running = True
        self.thread = threading.Thread(target=run, daemon=True)
        self.thread.start()