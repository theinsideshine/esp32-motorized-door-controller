import sys

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QApplication,
    QDoubleSpinBox,
    QFormLayout,
    QFrame,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QVBoxLayout,
    QWidget,
)

from ui.door_position_widget import DoorPositionWidget


class DemoWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("DoorPositionWidget Demo")
        self.resize(1050, 560)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)
        layout.setContentsMargins(24, 24, 24, 24)
        layout.setSpacing(18)

        title = QLabel("DoorPositionWidget · Demo aislada")
        title.setStyleSheet("font-size: 20px; font-weight: 600; color: #17212B;")
        layout.addWidget(title)

        self.position_widget = DoorPositionWidget()
        self.position_widget.positionSelected.connect(self._on_position_selected)
        layout.addWidget(self.position_widget)

        controls = QFrame()
        controls.setFrameShape(QFrame.StyledPanel)
        controls_layout = QHBoxLayout(controls)
        controls_layout.setContentsMargins(18, 14, 18, 14)
        controls_layout.setSpacing(26)

        form = QFormLayout()
        self.inputs = {}
        initial_values = {
            "pos1_deg": 2.29,
            "pos2_deg": 291.23,
            "pos3_deg": 206.06,
            "travel": 152.40,
            "setpoint": 206.06,
        }
        for key, value in initial_values.items():
            spin = QDoubleSpinBox()
            spin.setRange(-10000.0, 10000.0)
            spin.setDecimals(2)
            spin.setSingleStep(1.0)
            spin.setSuffix("°")
            spin.setValue(value)
            spin.setMinimumWidth(130)
            spin.valueChanged.connect(self._update_widget)
            self.inputs[key] = spin
            form.addRow(key, spin)

        controls_layout.addLayout(form)
        controls_layout.addStretch()

        self.selection_label = QLabel("Selección: ninguna")
        self.selection_label.setAlignment(Qt.AlignCenter)
        self.selection_label.setMinimumWidth(260)
        self.selection_label.setStyleSheet(
            "background: #F4F6F8; color: #52606D; border-radius: 10px; "
            "padding: 16px; font-size: 14px; font-weight: 600;"
        )
        controls_layout.addWidget(self.selection_label)
        layout.addWidget(controls)

        self.setStyleSheet("QMainWindow, QWidget { background-color: #F4F6F8; }")

    def _update_widget(self):
        self.position_widget.set_positions(
            self.inputs["pos1_deg"].value(),
            self.inputs["pos2_deg"].value(),
            self.inputs["pos3_deg"].value(),
        )
        self.position_widget.set_telemetry(
            self.inputs["travel"].value(),
            self.inputs["setpoint"].value(),
        )

    def _on_position_selected(self, position_key):
        value = self.position_widget.selected_value()
        self.inputs["setpoint"].setValue(value)
        self.selection_label.setText(f"Selección: {position_key}\n{value:.2f}°")


def main():
    app = QApplication(sys.argv)
    window = DemoWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
