import math

from PySide6.QtCore import Property, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QFont, QMouseEvent, QPainter, QPen
from PySide6.QtWidgets import QWidget


class DoorPositionWidget(QWidget):
    positionSelected = Signal(str)

    _POSITION_KEYS = ("pos1_deg", "pos2_deg", "pos3_deg")
    _POSITION_LABELS = {
        "pos1_deg": "POS_1",
        "pos2_deg": "POS_2",
        "pos3_deg": "POS_3",
    }

    def __init__(self, parent=None):
        super().__init__(parent)
        self._positions = {
            "pos1_deg": 2.29,
            "pos2_deg": 291.23,
            "pos3_deg": 206.06,
        }
        self._travel = 152.40
        self._setpoint = 206.06
        self._selected_position = None
        self._interaction_enabled = True
        self._station_hit_areas = {}

        self.setMinimumSize(620, 260)
        self.setMouseTracking(True)

    def sizeHint(self):
        return super().sizeHint().expandedTo(self.minimumSize())

    def set_positions(self, pos1_deg, pos2_deg, pos3_deg):
        self._positions = {
            "pos1_deg": self._finite_float(pos1_deg, "pos1_deg"),
            "pos2_deg": self._finite_float(pos2_deg, "pos2_deg"),
            "pos3_deg": self._finite_float(pos3_deg, "pos3_deg"),
        }
        self.update()

    def set_pos1_deg(self, value):
        self._set_position("pos1_deg", value)

    def set_pos2_deg(self, value):
        self._set_position("pos2_deg", value)

    def set_pos3_deg(self, value):
        self._set_position("pos3_deg", value)

    def set_telemetry(self, travel, setpoint):
        self._travel = self._finite_float(travel, "travel")
        self._setpoint = self._finite_float(setpoint, "setpoint")
        self.update()

    def set_travel(self, value):
        self._travel = self._finite_float(value, "travel")
        self.update()

    def set_setpoint(self, value):
        self._setpoint = self._finite_float(value, "setpoint")
        self.update()

    def set_selected_position(self, position_key):
        if position_key is not None and position_key not in self._POSITION_KEYS:
            raise ValueError(f"Unknown position key: {position_key}")
        self._selected_position = position_key
        self.update()

    def clear_selection(self):
        self.set_selected_position(None)

    def selected_position(self):
        return self._selected_position

    def selected_value(self):
        if self._selected_position is None:
            return None
        return self._positions[self._selected_position]

    def set_interaction_enabled(self, enabled):
        self._interaction_enabled = bool(enabled)
        self.setCursor(Qt.ArrowCursor)
        self.update()

    def interaction_enabled(self):
        return self._interaction_enabled

    pos1_deg = Property(float, lambda self: self._positions["pos1_deg"], set_pos1_deg)
    pos2_deg = Property(float, lambda self: self._positions["pos2_deg"], set_pos2_deg)
    pos3_deg = Property(float, lambda self: self._positions["pos3_deg"], set_pos3_deg)
    travel = Property(float, lambda self: self._travel, set_travel)
    setpoint = Property(float, lambda self: self._setpoint, set_setpoint)
    interactionEnabled = Property(bool, interaction_enabled, set_interaction_enabled)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.setRenderHint(QPainter.TextAntialiasing)

        outer = QRectF(self.rect()).adjusted(1, 1, -1, -1)
        painter.setPen(QPen(QColor("#DDE3E8"), 1))
        painter.setBrush(QColor("#FFFFFF"))
        painter.drawRoundedRect(outer, 16, 16)

        self._draw_header(painter, outer)
        track = QRectF(outer.left() + 52, outer.top() + 145, outer.width() - 104, 6)
        range_min, range_max = self._display_range()
        station_points = {
            key: self._value_to_x(value, track, range_min, range_max)
            for key, value in self._positions.items()
        }

        self._draw_track(painter, track, range_min, range_max)
        self._draw_stations(painter, track, station_points)
        self._draw_setpoint(painter, track, range_min, range_max)
        self._draw_travel(painter, track, range_min, range_max)
        self._draw_readout(painter, outer)

    def mouseMoveEvent(self, event):
        if not self._interaction_enabled:
            self.setCursor(Qt.ArrowCursor)
            return
        over_station = any(area.contains(event.position()) for area in self._station_hit_areas.values())
        self.setCursor(Qt.PointingHandCursor if over_station else Qt.ArrowCursor)

    def mouseReleaseEvent(self, event: QMouseEvent):
        if event.button() != Qt.LeftButton or not self._interaction_enabled:
            return
        for key, area in self._station_hit_areas.items():
            if area.contains(event.position()):
                self.set_selected_position(key)
                self.positionSelected.emit(key)
                event.accept()
                return
        super().mouseReleaseEvent(event)

    def _draw_header(self, painter, outer):
        painter.setPen(QColor("#17212B"))
        painter.setFont(QFont("Segoe UI", 13, QFont.DemiBold))
        painter.drawText(QRectF(outer.left() + 24, outer.top() + 18, 260, 24), Qt.AlignLeft, "Control de posición")

        painter.setPen(QColor("#7A8793"))
        painter.setFont(QFont("Segoe UI", 9))
        painter.drawText(QRectF(outer.left() + 24, outer.top() + 42, 200, 18), Qt.AlignLeft, "Recorrido angular")

        status = "SIN DESTINO"
        if self._selected_position:
            status = f"DESTINO · {self._POSITION_LABELS[self._selected_position]}"
        painter.setPen(QColor("#247B8A") if self._selected_position else QColor("#7A8793"))
        painter.setFont(QFont("Segoe UI", 9, QFont.DemiBold))
        painter.drawText(
            QRectF(outer.right() - 250, outer.top() + 21, 226, 22),
            Qt.AlignRight | Qt.AlignVCenter,
            status,
        )

    def _draw_track(self, painter, track, range_min, range_max):
        y = track.center().y()
        painter.setPen(QPen(QColor("#CBD4DC"), 4, Qt.SolidLine, Qt.RoundCap))
        painter.drawLine(track.left(), y, track.right(), y)

        travel_x = self._value_to_x(self._travel, track, range_min, range_max, clamp=True)
        setpoint_x = self._value_to_x(self._setpoint, track, range_min, range_max, clamp=True)
        painter.setPen(QPen(QColor("#247B8A"), 6, Qt.SolidLine, Qt.RoundCap))
        painter.drawLine(travel_x, y, setpoint_x, y)

        if not math.isclose(travel_x, setpoint_x, abs_tol=1.0):
            direction = 1 if setpoint_x > travel_x else -1
            tip_x = (travel_x + setpoint_x) / 2 + direction * 7
            painter.setBrush(QColor("#247B8A"))
            painter.setPen(Qt.NoPen)
            painter.drawPolygon([
                self._point(tip_x, y),
                self._point(tip_x - direction * 10, y - 6),
                self._point(tip_x - direction * 10, y + 6),
            ])

    def _draw_stations(self, painter, track, station_points):
        self._station_hit_areas = {}
        sorted_stations = sorted(station_points.items(), key=lambda item: item[1])
        for index, (key, x) in enumerate(sorted_stations):
            selected = key == self._selected_position
            label_y = track.top() - 67 if index % 2 == 0 else track.top() - 50
            hit_area = QRectF(x - 54, label_y - 6, 108, track.center().y() - label_y + 28)
            self._station_hit_areas[key] = hit_area

            if selected:
                painter.setPen(Qt.NoPen)
                painter.setBrush(QColor("#EDF5F8"))
                painter.drawRoundedRect(QRectF(x - 48, label_y - 4, 96, 42), 10, 10)

            color = QColor("#247B8A") if selected else QColor("#52606D")
            painter.setPen(color)
            painter.setFont(QFont("Segoe UI", 9, QFont.DemiBold))
            painter.drawText(QRectF(x - 52, label_y, 104, 18), Qt.AlignCenter, self._POSITION_LABELS[key])
            painter.setFont(QFont("Segoe UI", 10, QFont.DemiBold))
            painter.drawText(QRectF(x - 52, label_y + 17, 104, 20), Qt.AlignCenter, f"{self._positions[key]:.2f}°")

            painter.setPen(QPen(QColor("#B6C1C9"), 1))
            painter.drawLine(x, label_y + 39, x, track.center().y() - 8)
            painter.setPen(QPen(color, 2))
            painter.setBrush(QColor("#FFFFFF"))
            painter.drawEllipse(self._point(x, track.center().y()), 6 if not selected else 8, 6 if not selected else 8)

    def _draw_travel(self, painter, track, range_min, range_max):
        x = self._value_to_x(self._travel, track, range_min, range_max, clamp=True)
        y = track.center().y()
        painter.setPen(QPen(QColor("#FFFFFF"), 3))
        painter.setBrush(QColor("#183B56"))
        painter.drawEllipse(self._point(x, y), 10, 10)

    def _draw_setpoint(self, painter, track, range_min, range_max):
        x = self._value_to_x(self._setpoint, track, range_min, range_max, clamp=True)
        y = track.center().y()
        painter.setPen(QPen(QColor("#247B8A"), 3))
        painter.setBrush(QColor("#FFFFFF"))
        painter.drawEllipse(self._point(x, y), 12, 12)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor("#247B8A"))
        painter.drawEllipse(self._point(x, y), 3, 3)

    def _draw_readout(self, painter, outer):
        bottom = outer.bottom() - 20
        left = outer.left() + 40
        right = outer.right() - 40

        painter.setPen(QColor("#7A8793"))
        painter.setFont(QFont("Segoe UI", 8, QFont.DemiBold))
        painter.drawText(QRectF(left, bottom - 54, 210, 16), Qt.AlignLeft, "POSICIÓN ACTUAL · travel")
        painter.drawText(QRectF(right - 210, bottom - 54, 210, 16), Qt.AlignRight, "DESTINO · setpoint")

        painter.setPen(QColor("#17212B"))
        painter.setFont(QFont("Segoe UI", 18, QFont.DemiBold))
        painter.drawText(QRectF(left, bottom - 38, 210, 32), Qt.AlignLeft | Qt.AlignVCenter, f"{self._travel:.2f}°")

        painter.setPen(QColor("#247B8A"))
        painter.drawText(
            QRectF(right - 210, bottom - 38, 210, 32),
            Qt.AlignRight | Qt.AlignVCenter,
            f"{self._setpoint:.2f}°",
        )

    def _display_range(self):
        values = tuple(self._positions.values())
        minimum = min(values)
        maximum = max(values)
        span = maximum - minimum
        margin = max(span * 0.08, 1.0)
        if math.isclose(span, 0.0):
            margin = max(abs(minimum) * 0.1, 10.0)
        return minimum - margin, maximum + margin

    @staticmethod
    def _value_to_x(value, track, range_min, range_max, clamp=False):
        ratio = (value - range_min) / (range_max - range_min)
        if clamp:
            ratio = max(0.0, min(1.0, ratio))
        return track.left() + ratio * track.width()

    def _set_position(self, key, value):
        self._positions[key] = self._finite_float(value, key)
        self.update()

    @staticmethod
    def _finite_float(value, name):
        converted = float(value)
        if not math.isfinite(converted):
            raise ValueError(f"{name} must be a finite number")
        return converted

    @staticmethod
    def _point(x, y):
        from PySide6.QtCore import QPointF

        return QPointF(x, y)
