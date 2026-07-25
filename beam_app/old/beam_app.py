import math
import sys
from pathlib import Path

from PySide6.QtCore import Qt, QRectF
from PySide6.QtGui import QPainter, QPen, QColor, QFont, QPixmap
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
    QProgressBar,
    QVBoxLayout,
    QWidget,
)

BASE_DIR = Path(__file__).resolve().parent
LOGO_PATH = BASE_DIR / "udemm_logo.png"


class GaugeWidget(QWidget):
    def __init__(
        self,
        min_value=0,
        max_value=100,
        value=0,
        unit="",
        decimals=1,
        accent="#4da3ff",
        parent=None,
    ):
        super().__init__(parent)
        self.min_value = min_value
        self.max_value = max_value
        self.value = value
        self.unit = unit
        self.decimals = decimals
        self.accent = QColor(accent)
        self.setMinimumSize(160, 160)

    def set_value(self, value):
        self.value = max(self.min_value, min(self.max_value, value))
        self.update()

    def set_range(self, min_value, max_value):
        self.min_value = min_value
        self.max_value = max_value
        self.value = max(self.min_value, min(self.max_value, self.value))
        self.update()

    def _value_to_angle(self):
        span = self.max_value - self.min_value
        if span <= 0:
            return -225
        ratio = (self.value - self.min_value) / span
        return -225 + ratio * 270

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        rect = self.rect().adjusted(10, 10, -10, -10)
        size = min(rect.width(), rect.height())
        cx = rect.center().x()
        cy = rect.center().y()

        gauge_rect = QRectF(cx - size / 2, cy - size / 2, size, size)

        # fondo circular
        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor("#101722"))
        painter.drawEllipse(gauge_rect)

        # aro exterior
        pen_outer = QPen(QColor("#253247"), 5)
        painter.setPen(pen_outer)
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(gauge_rect.adjusted(4, 4, -4, -4))

        # arco base
        arc_rect = gauge_rect.adjusted(18, 18, -18, -18)
        pen_bg = QPen(QColor("#1b2432"), 12)
        pen_bg.setCapStyle(Qt.RoundCap)
        painter.setPen(pen_bg)
        painter.drawArc(arc_rect, 225 * 16, -270 * 16)

        # arco valor
        value_angle = self._value_to_angle()
        span_angle = value_angle + 225
        pen_val = QPen(self.accent, 12)
        pen_val.setCapStyle(Qt.RoundCap)
        painter.setPen(pen_val)
        painter.drawArc(arc_rect, 225 * 16, -span_angle * 16)

        # marcas
        painter.save()
        painter.translate(cx, cy)
        for i in range(6):
            angle_deg = -225 + i * 54
            angle_rad = math.radians(angle_deg)
            r1 = size * 0.33
            r2 = size * 0.39
            x1 = math.cos(angle_rad) * r1
            y1 = math.sin(angle_rad) * r1
            x2 = math.cos(angle_rad) * r2
            y2 = math.sin(angle_rad) * r2
            pen_tick = QPen(QColor("#51627e"), 2)
            painter.setPen(pen_tick)
            painter.drawLine(int(x1), int(y1), int(x2), int(y2))
        painter.restore()

        # aguja
        painter.save()
        painter.translate(cx, cy)
        painter.rotate(value_angle)
        pen_needle = QPen(QColor("#f2f4f8"), 3)
        pen_needle.setCapStyle(Qt.RoundCap)
        painter.setPen(pen_needle)
        painter.drawLine(0, 0, int(size * 0.30), 0)
        painter.restore()

        # centro
        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor("#e8edf7"))
        painter.drawEllipse(rect.center(), 6, 6)

        # texto central
        painter.setPen(QColor("#ffffff"))
        font_value = QFont("Segoe UI", 13, QFont.Bold)
        painter.setFont(font_value)
        value_text = f"{self.value:.{self.decimals}f}"
        painter.drawText(
            QRectF(cx - size * 0.28, cy + size * 0.08, size * 0.56, 26),
            Qt.AlignCenter,
            value_text,
        )

        painter.setPen(QColor("#8fbfff"))
        font_unit = QFont("Segoe UI", 9, QFont.DemiBold)
        painter.setFont(font_unit)
        painter.drawText(
            QRectF(cx - size * 0.28, cy + size * 0.22, size * 0.56, 18),
            Qt.AlignCenter,
            self.unit,
        )


class OutputCard(QFrame):
    def __init__(
        self,
        title: str,
        value: str,
        percent: int = 0,
        gauge=False,
        gauge_min=0,
        gauge_max=100,
        gauge_value=0,
        gauge_unit="",
        gauge_decimals=1,
        gauge_accent="#4da3ff",
    ):
        super().__init__()
        self.setObjectName("outputCard")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 12, 14, 12)
        layout.setSpacing(8)

        self.title_label = QLabel(title)
        self.title_label.setObjectName("cardTitle")

        self.value_label = QLabel(value)
        self.value_label.setObjectName("cardValue")
        self.value_label.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)

        self.gauge = None
        if gauge:
            self.gauge = GaugeWidget(
                min_value=gauge_min,
                max_value=gauge_max,
                value=gauge_value,
                unit=gauge_unit,
                decimals=gauge_decimals,
                accent=gauge_accent,
            )
            self.gauge.setFixedSize(170, 170)

        self.progress = QProgressBar()
        self.progress.setObjectName("cardProgress")
        self.progress.setRange(0, 100)
        self.progress.setValue(percent)
        self.progress.setTextVisible(False)
        self.progress.setFixedHeight(10)

        layout.addWidget(self.title_label)
        layout.addWidget(self.value_label)

        if self.gauge:
            gauge_wrap = QHBoxLayout()
            gauge_wrap.addStretch()
            gauge_wrap.addWidget(self.gauge)
            gauge_wrap.addStretch()
            layout.addLayout(gauge_wrap)

        layout.addWidget(self.progress)

    def set_value(self, value: str):
        self.value_label.setText(value)

    def set_percent(self, percent: int):
        self.progress.setValue(max(0, min(100, percent)))

    def set_gauge_value(self, value: float):
        if self.gauge:
            self.gauge.set_value(value)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Banco de Ensayo - Viga Simplemente Apoyada")
        self.resize(1320, 820)
        self.setMinimumSize(1100, 720)

        self.setStyleSheet("""
            QMainWindow {
                background-color: #0f1115;
            }

            QWidget {
                background-color: #0f1115;
                color: #f2f4f8;
                font-family: "Segoe UI";
                font-size: 13px;
            }

            QLabel#subtitleLabel {
                color: #cfd6e4;
                font-size: 16px;
                font-weight: 600;
            }

            QLabel#titleLabel {
                color: white;
                font-size: 28px;
                font-weight: 800;
            }

            QLabel#sectionTitle {
                color: white;
                font-size: 16px;
                font-weight: 700;
            }

            QLabel#inputLabel {
                color: #71b0ff;
                font-size: 14px;
                font-weight: 700;
            }

            QLabel#statusLabel {
                color: #4da3ff;
                font-size: 14px;
                font-weight: 700;
            }

            QPushButton {
                background-color: #1e2633;
                color: white;
                border: 1px solid #394355;
                border-radius: 10px;
                padding: 10px 18px;
                font-size: 13px;
                font-weight: 600;
                min-height: 18px;
            }

            QPushButton:hover {
                background-color: #273143;
            }

            QPushButton:pressed {
                background-color: #18202c;
            }

            QPushButton#primaryButton {
                background-color: #1b4fa3;
                border: 1px solid #4d8dff;
            }

            QPushButton#primaryButton:hover {
                background-color: #2461c7;
            }

            QPushButton#successButton {
                background-color: #1f6a3b;
                border: 1px solid #4ad27f;
            }

            QPushButton#successButton:hover {
                background-color: #27854a;
            }

            QComboBox, QLineEdit {
                background-color: #161c26;
                color: white;
                border: 1px solid #3b4557;
                border-radius: 8px;
                padding: 8px 10px;
                min-height: 20px;
            }

            QComboBox::drop-down {
                border: none;
            }

            QFrame#panelFrame {
                background-color: #11161f;
                border: 1px solid #242c3a;
                border-radius: 16px;
            }

            QFrame#groupFrame {
                background-color: #151b25;
                border: 1px solid #262f3f;
                border-radius: 14px;
            }

            QLabel#logoBox {
                background-color: transparent;
                border: none;
            }

            QFrame#outputCard {
                background-color: #171f2b;
                border: 1px solid #2a3550;
                border-radius: 14px;
            }

            QLabel#cardTitle {
                color: #71b0ff;
                font-size: 13px;
                font-weight: 700;
            }

            QLabel#cardValue {
                color: white;
                font-size: 24px;
                font-weight: 800;
            }

            QProgressBar#cardProgress {
                background-color: #0e131b;
                border: 1px solid #243043;
                border-radius: 5px;
            }

            QProgressBar#cardProgress::chunk {
                background-color: #4da3ff;
                border-radius: 4px;
            }
        """)

        central = QWidget()
        self.setCentralWidget(central)

        root = QVBoxLayout(central)
        root.setContentsMargins(20, 20, 20, 20)
        root.setSpacing(16)

        root.addLayout(self._build_header())
        root.addWidget(self._build_center(), 1)
        root.addLayout(self._build_footer())

        # demo visual inicial
        self.card_r1.set_value("0.0 N")
        self.card_r1.set_gauge_value(0.0)

        self.card_r2.set_value("0.0 N")
        self.card_r2.set_gauge_value(0.0)

        self.card_flex.set_value("0.0 mm")
        self.card_flex.set_gauge_value(0.0)

    def _build_header(self):
        layout = QHBoxLayout()
        layout.setSpacing(14)

        left_layout = QHBoxLayout()
        left_layout.setSpacing(14)

        logo_label = QLabel()
        logo_label.setObjectName("logoBox")
        logo_label.setFixedSize(150, 90)
        logo_label.setAlignment(Qt.AlignCenter)

        if LOGO_PATH.exists():
            pixmap = QPixmap(str(LOGO_PATH))
            scaled = pixmap.scaled(
                150, 90,
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation
            )
            logo_label.setPixmap(scaled)
        else:
            logo_label.setText("LOGO")

        left_layout.addWidget(logo_label)

        titles_layout = QVBoxLayout()
        titles_layout.setSpacing(2)

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
        main_layout.setContentsMargins(16, 16, 16, 16)
        main_layout.setSpacing(16)

        top_grid = QGridLayout()
        top_grid.setHorizontalSpacing(16)
        top_grid.setVerticalSpacing(16)

        input_frame = self._build_input_panel()
        output_frame = self._build_output_panel()

        top_grid.addWidget(input_frame, 0, 0)
        top_grid.addWidget(output_frame, 0, 1)

        top_grid.setColumnStretch(0, 2)
        top_grid.setColumnStretch(1, 3)

        main_layout.addLayout(top_grid)

        return panel

    def _build_input_panel(self):
        frame = QFrame()
        frame.setObjectName("groupFrame")
        frame.setMaximumWidth(520)

        layout = QVBoxLayout(frame)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)

        title = QLabel("Entradas del ensayo")
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        layout.addLayout(self._input_row("Distancia (mm)", "0"))
        layout.addLayout(self._input_combo_row("Carga (kg)", ["0", "1", "2", "5", "10"], "0"))
        layout.addStretch()

        return frame

    def _build_output_panel(self):
        frame = QFrame()
        frame.setObjectName("groupFrame")

        layout = QVBoxLayout(frame)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)

        title = QLabel("Salidas del ensayo")
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        cards_grid = QGridLayout()
        cards_grid.setHorizontalSpacing(12)
        cards_grid.setVerticalSpacing(12)

        self.card_r1 = OutputCard(
            "Fuerza de reacción 1",
            "0.0 N",
            0,
            gauge=True,
            gauge_min=0,
            gauge_max=100,
            gauge_value=0,
            gauge_unit="N",
            gauge_decimals=1,
            gauge_accent="#4da3ff",
        )

        self.card_r2 = OutputCard(
            "Fuerza de reacción 2",
            "0.0 N",
            0,
            gauge=True,
            gauge_min=0,
            gauge_max=100,
            gauge_value=0,
            gauge_unit="N",
            gauge_decimals=1,
            gauge_accent="#67d4ff",
        )

        self.card_flex = OutputCard(
            "Flexión",
            "0.0 mm",
            0,
            gauge=True,
            gauge_min=0,
            gauge_max=20,
            gauge_value=0,
            gauge_unit="mm",
            gauge_decimals=2,
            gauge_accent="#7aa2ff",
        )

        self.card_status = OutputCard("Status del ensayo", "Listo", 100)

        cards_grid.addWidget(self.card_r1, 0, 0)
        cards_grid.addWidget(self.card_r2, 0, 1)
        cards_grid.addWidget(self.card_flex, 1, 0)
        cards_grid.addWidget(self.card_status, 1, 1)

        layout.addLayout(cards_grid)
        layout.addStretch()

        return frame

    def _build_footer(self):
        layout = QHBoxLayout()
        layout.setSpacing(10)

        puerto_label = QLabel("Puerto:")
        puerto_combo = QComboBox()
        puerto_combo.addItems(["COM3", "COM4", "COM5", "/dev/ttyACM0"])
        puerto_combo.setCurrentText("COM3")
        puerto_combo.setFixedWidth(120)

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
        label.setMinimumWidth(150)

        field = QLineEdit()
        field.setText(default_value)
        field.setMaximumWidth(220)

        row.addWidget(label)
        row.addWidget(field)
        row.addStretch()

        return row

    def _input_combo_row(self, label_text, values, default_value):
        row = QHBoxLayout()
        row.setSpacing(10)

        label = QLabel(f"{label_text}:")
        label.setObjectName("inputLabel")
        label.setMinimumWidth(150)

        combo = QComboBox()
        combo.addItems(values)
        combo.setCurrentText(default_value)
        combo.setMaximumWidth(220)

        row.addWidget(label)
        row.addWidget(combo)
        row.addStretch()

        return row


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()