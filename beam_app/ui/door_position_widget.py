import math

from PySide6.QtCore import Property, QPointF, QRectF, Qt, Signal
from PySide6.QtGui import QColor, QFont, QMouseEvent, QPainter, QPainterPath, QPen, QPolygonF
from PySide6.QtWidgets import QWidget


class DoorPositionWidget(QWidget):
    """Modern 0..360 degree position view for an AS5048A absolute sensor."""

    positionSelected = Signal(str)
    _POSITION_KEYS = ("pos1_deg", "pos2_deg", "pos3_deg")
    _LABELS = {"pos1_deg": "POS_1", "pos2_deg": "POS_2", "pos3_deg": "POS_3"}

    def __init__(self, parent=None):
        super().__init__(parent)
        self._positions = {"pos1_deg": 2.29, "pos2_deg": 291.23, "pos3_deg": 206.06}
        self._travel = 291.23
        self._setpoint = 291.23
        self._arrival_tol = 2.0
        self._direction = "NONE"
        self._presentation_state = None
        self._selected_position = None
        self._interaction_enabled = True
        self._hit_areas = {}
        self.setMinimumSize(620, 430)
        self.setMouseTracking(True)

    def set_positions(self, pos1_deg, pos2_deg, pos3_deg):
        self._positions = {k: self._normalize(v) for k, v in zip(self._POSITION_KEYS, (pos1_deg, pos2_deg, pos3_deg))}
        self.update()

    def set_telemetry(self, travel, setpoint, direction=None, arrival_tol=None):
        self._travel = self._normalize(travel)
        self._setpoint = self._normalize(setpoint)
        if direction is not None:
            self._direction = str(direction).upper()
        if arrival_tol is not None:
            self._arrival_tol = max(0.0, float(arrival_tol))
        self.update()

    def set_presentation_state(self, state):
        """Set an optional local simulation state used only for presentation."""
        self._presentation_state = state
        self.update()

    def set_travel(self, value):
        self._travel = self._normalize(value); self.update()

    def set_setpoint(self, value):
        self._setpoint = self._normalize(value); self.update()

    def set_pos1_deg(self, value): self._set_position("pos1_deg", value)
    def set_pos2_deg(self, value): self._set_position("pos2_deg", value)
    def set_pos3_deg(self, value): self._set_position("pos3_deg", value)

    def set_selected_position(self, key):
        if key is not None and key not in self._POSITION_KEYS:
            raise ValueError(f"Unknown position key: {key}")
        self._selected_position = key
        self.update()

    def clear_selection(self): self.set_selected_position(None)
    def selected_position(self): return self._selected_position
    def selected_value(self): return None if self._selected_position is None else self._positions[self._selected_position]

    def set_interaction_enabled(self, enabled):
        self._interaction_enabled = bool(enabled); self.update()

    def interaction_enabled(self): return self._interaction_enabled

    pos1_deg = Property(float, lambda s: s._positions["pos1_deg"], set_pos1_deg)
    pos2_deg = Property(float, lambda s: s._positions["pos2_deg"], set_pos2_deg)
    pos3_deg = Property(float, lambda s: s._positions["pos3_deg"], set_pos3_deg)
    travel = Property(float, lambda s: s._travel, set_travel)
    setpoint = Property(float, lambda s: s._setpoint, set_setpoint)
    interactionEnabled = Property(bool, interaction_enabled, set_interaction_enabled)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHints(QPainter.Antialiasing | QPainter.TextAntialiasing)
        outer = QRectF(self.rect()).adjusted(1, 1, -1, -1)
        painter.setPen(QPen(QColor("#2b3b50"), 1))
        painter.setBrush(QColor("#121a26"))
        painter.drawRoundedRect(outer, 16, 16)
        self._draw_header(painter, outer)
        side = min(outer.width() * 0.48, outer.height() - 96)
        center = QPointF(outer.left() + outer.width() * 0.33, outer.top() + 64 + side / 2)
        radius = side * 0.40
        self._draw_dial(painter, center, radius)
        self._draw_readout(painter, outer, center, radius)

    def _draw_header(self, painter, outer):
        painter.setPen(QColor("#eef3f8")); painter.setFont(QFont("Segoe UI", 13, QFont.DemiBold))
        painter.drawText(QRectF(outer.left()+20, outer.top()+16, 330, 25), Qt.AlignLeft, "Posición angular absoluta · AS5048A")
        painter.setPen(QColor("#7f91a9")); painter.setFont(QFont("Segoe UI", 9))
        subtitle = "0°–360° · haga clic en una posición" if self._interaction_enabled else "0°–360° · vista informativa · selección deshabilitada"
        painter.drawText(QRectF(outer.left()+20, outer.top()+40, 420, 18), Qt.AlignLeft, subtitle)

    def _draw_dial(self, painter, center, radius):
        painter.setPen(QPen(QColor("#2b3b50"), 12, Qt.SolidLine, Qt.RoundCap))
        painter.setBrush(Qt.NoBrush)
        painter.drawEllipse(center, radius, radius)
        for angle in range(0, 360, 30):
            inner = self._point(center, radius-15, angle); outer = self._point(center, radius+1, angle)
            painter.setPen(QPen(QColor("#506078"), 1 if angle % 90 else 2))
            painter.drawLine(inner, outer)
        painter.setFont(QFont("Segoe UI", 8)); painter.setPen(QColor("#77869a"))
        for angle in (0, 90, 180, 270):
            p = self._point(center, radius-31, angle)
            painter.drawText(QRectF(p.x()-22, p.y()-9, 44, 18), Qt.AlignCenter, f"{angle}°")

        self._hit_areas = {}
        for key, angle in self._positions.items():
            p = self._point(center, radius+2, angle)
            selected = key == self._selected_position
            color = QColor("#64d5ed") if selected else QColor("#8ea4bd")
            painter.setPen(QPen(color, 2)); painter.setBrush(QColor("#121a26"))
            painter.drawEllipse(p, 8 if selected else 6, 8 if selected else 6)
            label = self._point(center, radius+32, angle)
            rect = QRectF(label.x()-48, label.y()-18, 96, 36)
            self._hit_areas[key] = rect.adjusted(-6, -6, 6, 6).united(QRectF(p.x()-15, p.y()-15, 30, 30))
            painter.setPen(color); painter.setFont(QFont("Segoe UI", 8, QFont.DemiBold))
            painter.drawText(rect, Qt.AlignCenter, f"{self._LABELS[key]}\n{angle:.2f}°")

        # Setpoint: outlined diamond. Travel: solid marker. Both naturally wrap at 0/360.
        sp = self._point(center, radius, self._setpoint)
        diamond = QPolygonF([QPointF(sp.x(), sp.y()-10), QPointF(sp.x()+10, sp.y()),
                             QPointF(sp.x(), sp.y()+10), QPointF(sp.x()-10, sp.y())])
        painter.setPen(QPen(QColor("#f4c95d"), 3)); painter.setBrush(QColor("#121a26")); painter.drawPolygon(diamond)
        tr = self._point(center, radius, self._travel)
        painter.setPen(QPen(QColor("#dff7ff"), 2)); painter.setBrush(QColor("#2ea9d2")); painter.drawEllipse(tr, 9, 9)

        delta = self._signed_delta(self._travel, self._setpoint)
        arrived = abs(delta) <= self._arrival_tol
        painter.setPen(Qt.NoPen); painter.setBrush(self._center_color(arrived))
        painter.drawEllipse(center, radius*0.50, radius*0.50)
        painter.setPen(QColor("#ffffff")); painter.setFont(QFont("Segoe UI", 20, QFont.Bold))
        painter.drawText(QRectF(center.x()-75, center.y()-31, 150, 32), Qt.AlignCenter, f"{self._travel:.2f}°")
        painter.setFont(QFont("Segoe UI", 9, QFont.DemiBold))
        painter.drawText(QRectF(center.x()-90, center.y()+4, 180, 28), Qt.AlignCenter,
                         self._presentation_text(delta, arrived))

    def _draw_readout(self, painter, outer, center, radius):
        left = outer.left() + outer.width()*0.62
        width = outer.right() - left - 22
        rows = [("TRAVEL · marcador sólido", f"{self._travel:.2f}°", "#62c8eb"),
                ("SETPOINT · rombo", f"{self._setpoint:.2f}°", "#f4c95d"),
                ("ERROR ANGULAR MÍNIMO", f"{self._signed_delta(self._travel, self._setpoint):+.2f}°", "#eef3f8"),
                ("SENTIDO", self._presentation_text(self._signed_delta(self._travel, self._setpoint),
                                                     abs(self._signed_delta(self._travel, self._setpoint)) <= self._arrival_tol), "#eef3f8"),
                ("ARRIVAL TOL", f"± {self._arrival_tol:.2f}°", "#eef3f8")]
        y = outer.top()+78
        for caption, value, color in rows:
            painter.setPen(QColor("#7f91a9")); painter.setFont(QFont("Segoe UI", 8, QFont.DemiBold))
            painter.drawText(QRectF(left, y, width, 16), Qt.AlignLeft, caption)
            painter.setPen(QColor(color)); painter.setFont(QFont("Segoe UI", 14, QFont.DemiBold))
            painter.drawText(QRectF(left, y+16, width, 26), Qt.AlignLeft, value)
            y += 55

    def mouseMoveEvent(self, event):
        over = self._interaction_enabled and any(r.contains(event.position()) for r in self._hit_areas.values())
        self.setCursor(Qt.PointingHandCursor if over else Qt.ArrowCursor)

    def mouseReleaseEvent(self, event: QMouseEvent):
        if event.button() == Qt.LeftButton and self._interaction_enabled:
            for key, rect in self._hit_areas.items():
                if rect.contains(event.position()):
                    self.set_selected_position(key); self.positionSelected.emit(key); return
        super().mouseReleaseEvent(event)

    def _set_position(self, key, value): self._positions[key] = self._normalize(value); self.update()

    @staticmethod
    def _normalize(value):
        value = float(value)
        if not math.isfinite(value): raise ValueError("angle must be finite")
        return value % 360.0

    @staticmethod
    def _point(center, radius, angle):
        radians = math.radians(angle-90)
        return QPointF(center.x()+math.cos(radians)*radius, center.y()+math.sin(radians)*radius)

    @staticmethod
    def _signed_delta(start, end): return (end-start+180.0) % 360.0 - 180.0

    def _direction_text(self, delta):
        if abs(delta) <= self._arrival_tol: return "LLEGADA CONFIRMADA"
        if self._direction in ("CW", "HORARIO"): return "HORARIO · CW"
        if self._direction in ("CCW", "ANTIHORARIO"): return "ANTIHORARIO · CCW"
        return "HORARIO · CW" if delta > 0 else "ANTIHORARIO · CCW"

    def _center_color(self, arrived):
        if self._presentation_state in ("IDLE", "IDLE_POS1", "IDLE_POS2", "IDLE_POS3"):
            return QColor("#287fb8")
        if self._presentation_state in (
            "FWD_ACTIVE", "REW_ACTIVE", "GO_POS1_ACTIVE", "GO_POS2_ACTIVE", "GO_POS3_ACTIVE"
        ):
            return QColor("#3cc986")
        if self._presentation_state in ("STOPPED", "ERROR"):
            return QColor("#c93c49")
        return QColor("#3cc986" if arrived else "#243348")

    def _presentation_text(self, delta, arrived):
        texts = {
            "IDLE": "DETENIDA / EN REPOSO",
            "FWD_ACTIVE": "FWD · HACIA POS_1",
            "REW_ACTIVE": "REW · HACIA POS_3",
            "STOPPED": "DETENIDA POR STOP",
            "ERROR": "ERROR",
            "GO_POS1_ACTIVE": "HACIA POS_1",
            "GO_POS2_ACTIVE": "HACIA POS_2",
            "GO_POS3_ACTIVE": "HACIA POS_3",
            "IDLE_POS1": "IDLE EN POS_1",
            "IDLE_POS2": "IDLE EN POS_2",
            "IDLE_POS3": "IDLE EN POS_3",
        }
        if self._presentation_state in texts:
            return texts[self._presentation_state]
        return "EN TOLERANCIA" if arrived else self._direction_text(delta)
