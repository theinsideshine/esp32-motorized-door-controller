import sys
from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QPushButton,
    QVBoxLayout,
    QWidget,
)

BASE_DIR = Path(__file__).resolve().parent
LOGO_PATH = BASE_DIR / "udemm_logo.png"
BEAM_PATH = BASE_DIR / "beam.png"


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Banco de Ensayo - Viga Simplemente Apoyada")
        self.resize(1280, 760)
        self.setMinimumSize(1050, 680)

        self.setStyleSheet("""
            QMainWindow {
                background-color: #111111;
            }

            QWidget {
                background-color: #111111;
                color: white;
                font-family: "Segoe UI";
                font-size: 13px;
            }

            QLabel#subtitleLabel {
                color: #f0f0f0;
                font-size: 16px;
                font-weight: 600;
            }

            QLabel#titleLabel {
                color: white;
                font-size: 22px;
                font-weight: 700;
            }

            QLabel#sectionTitle {
                color: white;
                font-size: 15px;
                font-weight: 700;
            }

            QLabel#inputLabel {
                color: #ff4d4d;
                font-size: 14px;
                font-weight: 700;
            }

            QLabel#outputLabel {
                color: #3f8cff;
                font-size: 14px;
                font-weight: 700;
            }

            QLabel#outputValue {
                color: #3f8cff;
                font-size: 14px;
                font-weight: 600;
            }

            QLabel#statusLabel {
                color: #3f8cff;
                font-size: 14px;
                font-weight: 700;
            }

            QPushButton {
                background-color: #2a2a2a;
                color: white;
                border: 1px solid #8a8a8a;
                padding: 10px 18px;
                font-size: 13px;
                min-height: 18px;
            }

            QPushButton:hover {
                background-color: #3a3a3a;
            }

            QPushButton:pressed {
                background-color: #1d1d1d;
            }

            QPushButton#primaryButton {
                background-color: #1f3b63;
                border: 1px solid #4f89d4;
            }

            QPushButton#primaryButton:hover {
                background-color: #294c7d;
            }

            QPushButton#successButton {
                background-color: #1f4a2c;
                border: 1px solid #66cc88;
            }

            QPushButton#successButton:hover {
                background-color: #286238;
            }

            QComboBox, QLineEdit {
                background-color: #1d1d1d;
                color: white;
                border: 1px solid #777777;
                padding: 6px 8px;
                min-height: 18px;
            }

            QFrame#panelFrame {
                background-color: #0d0d0d;
                border: 1px solid #555555;
            }

            QFrame#groupFrame {
                background-color: #121212;
                border: 1px solid #2e2e2e;
            }

            QLabel#logoBox {
                background-color: #111111;
                border: none;
            }

            QLabel#imageBox {
                background-color: #0f0f0f;
                border: 1px solid #2f2f2f;
            }
        """)

        central = QWidget()
        self.setCentralWidget(central)

        root = QVBoxLayout(central)
        root.setContentsMargins(18, 18, 18, 18)
        root.setSpacing(14)

        root.addLayout(self._build_header())
        root.addWidget(self._build_center(), 1)
        root.addLayout(self._build_footer())

    def _build_header(self):
        layout = QHBoxLayout()
        layout.setSpacing(14)

        left_layout = QHBoxLayout()
        left_layout.setSpacing(14)

        logo_label = QLabel()
        logo_label.setObjectName("logoBox")
        logo_label.setFixedSize(140, 90)
        logo_label.setAlignment(Qt.AlignCenter)

        if LOGO_PATH.exists():
            pixmap = QPixmap(str(LOGO_PATH))
            scaled = pixmap.scaled(
                140,
                90,
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation
            )
            logo_label.setPixmap(scaled)
        else:
            logo_label.setText("LOGO")

        left_layout.addWidget(logo_label)

        titles_layout = QVBoxLayout()
        titles_layout.setSpacing(4)

        subtitle = QLabel("FACULTAD DE INGENIERÍA - FÍSICA 1")
        subtitle.setObjectName("subtitleLabel")

        title = QLabel("VIGA SIMPLEMENTE APOYADA")
        title.setObjectName("titleLabel")

        titles_layout.addWidget(subtitle)
        titles_layout.addWidget(title)
        titles_layout.addStretch()

        left_layout.addLayout(titles_layout)
        left_layout.addStretch()

        right_layout = QHBoxLayout()
        right_layout.setSpacing(10)

        github_button = QPushButton("GitHub")
        version_label = QLabel("v0.1.0")

        right_layout.addWidget(github_button)
        right_layout.addWidget(version_label)

        layout.addLayout(left_layout, 1)
        layout.addLayout(right_layout)

        return layout

    def _build_center(self):
        panel = QFrame()
        panel.setObjectName("panelFrame")

        main_layout = QVBoxLayout(panel)
        main_layout.setContentsMargins(14, 14, 14, 14)
        main_layout.setSpacing(14)

        top_grid = QGridLayout()
        top_grid.setHorizontalSpacing(14)
        top_grid.setVerticalSpacing(14)

        input_frame = QFrame()
        input_frame.setObjectName("groupFrame")
        input_layout = QVBoxLayout(input_frame)
        input_layout.setContentsMargins(14, 14, 14, 14)
        input_layout.setSpacing(10)

        input_title = QLabel("Entradas del ensayo")
        input_title.setObjectName("sectionTitle")
        input_layout.addWidget(input_title)

        input_layout.addLayout(self._input_row("Distancia (mm)", "0"))
        input_layout.addLayout(self._input_combo_row("Carga (kg)", ["0", "1", "2", "5", "10"], "0"))
        input_layout.addStretch()

        output_frame = QFrame()
        output_frame.setObjectName("groupFrame")
        output_layout = QVBoxLayout(output_frame)
        output_layout.setContentsMargins(14, 14, 14, 14)
        output_layout.setSpacing(10)

        output_title = QLabel("Salidas del ensayo")
        output_title.setObjectName("sectionTitle")
        output_layout.addWidget(output_title)

        output_layout.addLayout(self._output_row("Fuerza de reacción 1", "Sin fuerza R1"))
        output_layout.addLayout(self._output_row("Fuerza de reacción 2", "Sin fuerza R2"))
        output_layout.addLayout(self._output_row("Flexión", "Sin distancia de flexión"))
        output_layout.addLayout(self._output_row("Status del ensayo", "Sin status"))
        output_layout.addStretch()

        top_grid.addWidget(input_frame, 0, 0)
        top_grid.addWidget(output_frame, 0, 1)

        visual_frame = QFrame()
        visual_frame.setObjectName("groupFrame")
        visual_layout = QVBoxLayout(visual_frame)
        visual_layout.setContentsMargins(12, 12, 12, 12)

        center_label = QLabel("Área de visualización del ensayo")
        center_label.setObjectName("outputLabel")
        center_label.setAlignment(Qt.AlignCenter)

        visual_layout.addStretch()
        visual_layout.addWidget(center_label)
        visual_layout.addStretch()

        main_layout.addLayout(top_grid)
        main_layout.addWidget(visual_frame, 1)

        return panel

    def _build_footer(self):
        layout = QHBoxLayout()
        layout.setSpacing(10)

        puerto_label = QLabel("Puerto:")
        puerto_combo = QComboBox()
        puerto_combo.addItems(["COM3", "COM4", "COM5", "/dev/ttyACM0"])
        puerto_combo.setCurrentText("COM3")

        btn_actualizar = QPushButton("Actualizar COMs")
        btn_conectar = QPushButton("Conectar")
        btn_desconectar = QPushButton("Desconectar")
        btn_iniciar = QPushButton("Iniciar")
        btn_refrescar = QPushButton("Refrescar")

        btn_iniciar.setObjectName("primaryButton")
        btn_refrescar.setObjectName("successButton")

        status_label = QLabel("Estado: GUI sin lógica")
        status_label.setObjectName("statusLabel")

        layout.addWidget(puerto_label)
        layout.addWidget(puerto_combo)
        layout.addSpacing(12)
        layout.addWidget(btn_actualizar)
        layout.addWidget(btn_conectar)
        layout.addWidget(btn_desconectar)
        layout.addWidget(btn_iniciar)
        layout.addWidget(btn_refrescar)
        layout.addStretch()
        layout.addWidget(status_label)

        return layout

    def _input_row(self, label_text, default_value):
        row = QHBoxLayout()
        row.setSpacing(10)

        label = QLabel(f"{label_text}:")
        label.setObjectName("inputLabel")

        field = QLineEdit()
        field.setText(default_value)

        row.addWidget(label, 2)
        row.addWidget(field, 1)
        return row

    def _input_combo_row(self, label_text, values, default_value):
        row = QHBoxLayout()
        row.setSpacing(10)

        label = QLabel(f"{label_text}:")
        label.setObjectName("inputLabel")

        combo = QComboBox()
        combo.addItems(values)
        combo.setCurrentText(default_value)

        row.addWidget(label, 2)
        row.addWidget(combo, 1)
        return row

    def _output_row(self, label_text, value_text):
        row = QHBoxLayout()
        row.setSpacing(8)

        label = QLabel(f"{label_text}:")
        label.setObjectName("outputLabel")

        value = QLabel(value_text)
        value.setObjectName("outputValue")

        row.addWidget(label, 1)
        row.addWidget(value, 1)
        row.addStretch()

        return row


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()