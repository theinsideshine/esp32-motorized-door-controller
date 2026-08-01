import sys
import json
from pathlib import Path
from PySide6.QtWidgets import QApplication

from ui.main_window import MainWindow
from controllers.main_controller import MainController


def main():
    app = QApplication(sys.argv)
    data_path = Path(__file__).resolve().parent / "config" / "simulation_data.json"
    with data_path.open("r", encoding="utf-8") as source:
        simulation_data = json.load(source)

    window = MainWindow(simulation_data)
    controller = MainController(window, simulation_data)

    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
