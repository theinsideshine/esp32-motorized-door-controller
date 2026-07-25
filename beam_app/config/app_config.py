from pathlib import Path
import platform


# =========================
# RUTAS
# =========================
BASE_DIR = Path(__file__).resolve().parent.parent
ASSETS_DIR = BASE_DIR / "assets"
IMAGES_DIR = ASSETS_DIR / "images"
LOGO_PATH = IMAGES_DIR / "udemm_logo.png"
BEAM_DIAGRAM_PATH = IMAGES_DIR / "beam_diagram_dark.png"


# =========================
# DATOS DE APP
# =========================
WINDOW_TITLE = "Banco de Ensayo - Viga Simplemente Apoyada"
APP_SUBTITLE = "FACULTAD DE INGENIERÍA - FÍSICA 1"
APP_TITLE = "VIGA SIMPLEMENTE APOYADA"
APP_VERSION = "v2.0.0"
#v1 Primer test completo
#V2 queque entre serial y gui 
GITHUB_URL = "https://github.com/theinsideshine/beam_app"
SKYCIV_URL = "https://skyciv.com/es/free-beam-calculator/"

# =========================
# PLATAFORMA
# =========================
SYSTEM_NAME = platform.system().lower()
IS_WINDOWS = SYSTEM_NAME == "windows"
IS_LINUX = SYSTEM_NAME == "linux"


# =========================
# SERIAL
# =========================
DEFAULT_BAUDRATE = 115200
MSJ_INIT_COM = "Init Serial"

if IS_WINDOWS:
    DEFAULT_SERIAL_PORT = "COM3"
    SERIAL_PORT_OPTIONS = ["COM3", "COM4", "COM5", "COM6"]
elif IS_LINUX:
    DEFAULT_SERIAL_PORT = "/dev/ttyACM0"
    SERIAL_PORT_OPTIONS = ["/dev/ttyACM0", "/dev/ttyUSB0", "/dev/ttyUSB1"]
else:
    DEFAULT_SERIAL_PORT = ""
    SERIAL_PORT_OPTIONS = []


# =========================
# FLAGS VISUALES
# =========================
SHOW_FLEX_SECTION = False
SHOW_REACTION_GAUGES = True


# =========================
# TEXTOS DE UI
# =========================
SECTION_INPUTS = "Entradas del ensayo"
SECTION_OUTPUTS = "Salidas del ensayo"

LABEL_DISTANCE = "Distancia (mm)"
LABEL_LOAD = "Carga (g)"
LABEL_PORT = "Puerto:"

CARD_R1_TITLE = "Fuerza de reacción 1"
CARD_R2_TITLE = "Fuerza de reacción 2"
CARD_FLEX_TITLE = "Flexión"
CARD_STATUS_TITLE = "Status del ensayo"



BUTTON_GITHUB = "GitHub"
BUTTON_REFRESH_PORTS = "Actualizar COMs"
BUTTON_CONNECT = "Conectar"
BUTTON_DISCONNECT = "Desconectar"
BUTTON_START = "Iniciar"
BUTTON_REFRESH = "Refrescar"


# =========================
# VALORES INICIALES
# =========================
DEFAULT_DISTANCE = "500"
DEFAULT_LOAD = "2500"
LOAD_OPTIONS = ["0", "500", "1000", "1500", "2000", "2500", "3000", "3500", "4000", "4500", "5000"]


UNIT_FORCE = "g"
UNIT_FLEX = "mm"
REACTION_GAUGE_MIN = 0
REACTION_GAUGE_MAX = 8000
REACTION_GAUGE_INITIAL = 0

INITIAL_R1_VALUE = f"0.0 {UNIT_FORCE}"
INITIAL_R2_VALUE = f"0.0 {UNIT_FORCE}"
INITIAL_FLEX_VALUE = f"0.0 {UNIT_FLEX}"

# =========================
# ESTADOS DE ENSAYO
# =========================
STATE_DISCONNECTED = "Desconectado"
STATE_CONNECTED = "Conectado"
STATE_RUNNING = "En ejecución"

STATUS_BAR_STYLES = {
    STATE_DISCONNECTED: {
        "color": "#808080",   # gris
        "percent": 33,
    },
    STATE_CONNECTED: {
        "color": "#007BFF",   # azul
        "percent": 66,
    },
    STATE_RUNNING: {
        "color": "#28A745",   # verde
        "percent": 100,
    },
}

STATUS_READY = STATE_DISCONNECTED
STATUS_GUI_ONLY = STATE_DISCONNECTED
INITIAL_STATUS_VALUE = STATUS_READY
