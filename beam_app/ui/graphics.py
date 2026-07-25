import math

from PySide6.QtCore import Qt, QRectF
from PySide6.QtGui import QPainter, QPen, QColor, QFont
from PySide6.QtWidgets import QWidget, QFrame, QVBoxLayout, QLabel, QProgressBar, QHBoxLayout


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

        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor("#101722"))
        painter.drawEllipse(gauge_rect)

        pen_outer = QPen(QColor("#253247"), 5)
        painter.setPen(pen_outer)
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(gauge_rect.adjusted(4, 4, -4, -4))

        arc_rect = gauge_rect.adjusted(18, 18, -18, -18)
        pen_bg = QPen(QColor("#1b2432"), 12)
        pen_bg.setCapStyle(Qt.RoundCap)
        painter.setPen(pen_bg)
        painter.drawArc(arc_rect, 225 * 16, -270 * 16)

        value_angle = self._value_to_angle()
        span_angle = value_angle + 225
        pen_val = QPen(self.accent, 12)
        pen_val.setCapStyle(Qt.RoundCap)
        painter.setPen(pen_val)
        painter.drawArc(arc_rect, 225 * 16, -span_angle * 16)

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

        painter.save()
        painter.translate(cx, cy)
        painter.rotate(value_angle)
        pen_needle = QPen(QColor("#f2f4f8"), 3)
        pen_needle.setCapStyle(Qt.RoundCap)
        painter.setPen(pen_needle)
        painter.drawLine(0, 0, int(size * 0.30), 0)
        painter.restore()

        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor("#e8edf7"))
        painter.drawEllipse(rect.center(), 6, 6)

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

        # color inicial por defecto
        self.set_progress_color("#4da3ff")

    def set_value(self, value: str):
        self.value_label.setText(value)

    def set_percent(self, percent: int):
        self.progress.setValue(max(0, min(100, percent)))

    def set_gauge_value(self, value: float):
        if self.gauge:
            self.gauge.set_value(value)

    def set_progress_color(self, color: str):
        self.progress.setStyleSheet(f"""
            QProgressBar#cardProgress {{
                background-color: #0e131b;
                border: 1px solid #243043;
                border-radius: 5px;
            }}

            QProgressBar#cardProgress::chunk {{
                background-color: {color};
                border-radius: 4px;
            }}
        """)