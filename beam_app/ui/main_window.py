from PySide6.QtCore import Qt
from PySide6.QtGui import QPixmap, QCursor
from PySide6.QtWidgets import (
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

from config.app_config import (
    APP_SUBTITLE,
    APP_TITLE,
    APP_VERSION,
    BUTTON_CONNECT,
    BUTTON_DISCONNECT,
    BUTTON_GITHUB,
    BUTTON_REFRESH,
    BUTTON_REFRESH_PORTS,
    BUTTON_START,
    CARD_FLEX_TITLE,
    CARD_R1_TITLE,
    CARD_R2_TITLE,
    CARD_STATUS_TITLE,
    DEFAULT_DISTANCE,
    DEFAULT_LOAD,
    DEFAULT_SERIAL_PORT,
    INITIAL_FLEX_VALUE,
    INITIAL_R1_VALUE,
    INITIAL_R2_VALUE,
    INITIAL_STATUS_VALUE,
    LABEL_DISTANCE,
    LABEL_LOAD,
    LABEL_PORT,
    LOAD_OPTIONS,
    LOGO_PATH,
    SECTION_INPUTS,
    SECTION_OUTPUTS,
    SERIAL_PORT_OPTIONS,
    SHOW_FLEX_SECTION,
    STATUS_GUI_ONLY,
    WINDOW_TITLE,
    UNIT_FORCE,
    UNIT_FLEX,
    REACTION_GAUGE_MIN,
    REACTION_GAUGE_MAX,
    REACTION_GAUGE_INITIAL,
    BEAM_DIAGRAM_PATH,
)

from ui.graphics import OutputCard


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(WINDOW_TITLE)
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

            QFrame#beamCard {
                background-color: rgba(15, 23, 42, 0.55);
                border: 1px solid #223250;
                border-radius: 14px;
            }

            QLabel#beamImageLabel {
                background: transparent;
                border: none;
                padding: 4px;
                color: #cbd5e1;
            }

            QPushButton#skycivLinkButton {
                background: transparent;
                border: none;
                color: #4ea1ff;
                font-size: 15px;
                font-weight: 600;
                padding: 6px 10px;
            }

            QPushButton#skycivLinkButton:hover {
                color: #7cc0ff;
                text-decoration: underline;
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

        self.card_r1.set_value(INITIAL_R1_VALUE)
        self.card_r1.set_gauge_value(0.0)

        self.card_r2.set_value(INITIAL_R2_VALUE)
        self.card_r2.set_gauge_value(0.0)

        if self.card_flex is not None:
            self.card_flex.set_value(INITIAL_FLEX_VALUE)
            self.card_flex.set_gauge_value(0.0)

        self.card_status.set_value(INITIAL_STATUS_VALUE)

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
                150,
                90,
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation,
            )
            logo_label.setPixmap(scaled)
        else:
            logo_label.setText("LOGO")

        left_layout.addWidget(logo_label)

        titles_layout = QVBoxLayout()
        titles_layout.setSpacing(2)

        subtitle = QLabel(APP_SUBTITLE)
        subtitle.setObjectName("subtitleLabel")

        title = QLabel(APP_TITLE)
        title.setObjectName("titleLabel")

        titles_layout.addWidget(subtitle)
        titles_layout.addWidget(title)
        titles_layout.addStretch()

        left_layout.addLayout(titles_layout)
        left_layout.addStretch()

        right_layout = QHBoxLayout()
        right_layout.setSpacing(10)

        self.github_button = QPushButton(BUTTON_GITHUB)
        self.version_label = QLabel(APP_VERSION)

        right_layout.addWidget(self.github_button)
        right_layout.addWidget(self.version_label)

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

        title = QLabel(SECTION_INPUTS)
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        layout.addLayout(self._input_row(LABEL_DISTANCE, DEFAULT_DISTANCE))
        layout.addLayout(self._input_combo_row(LABEL_LOAD, LOAD_OPTIONS, DEFAULT_LOAD))
        layout.addWidget(self._build_beam_section())
        layout.addStretch()

        return frame

    def _build_beam_section(self):
        self.beam_card = QFrame()
        self.beam_card.setObjectName("beamCard")

        beam_layout = QVBoxLayout(self.beam_card)
        beam_layout.setContentsMargins(12, 12, 12, 12)
        beam_layout.setSpacing(10)

        self.beam_image_label = QLabel()
        self.beam_image_label.setObjectName("beamImageLabel")
        self.beam_image_label.setAlignment(Qt.AlignCenter)
        self.beam_image_label.setMinimumHeight(220)

        pixmap = QPixmap(str(BEAM_DIAGRAM_PATH))
        if not pixmap.isNull():
            scaled = pixmap.scaled(
                430,
                230,
                Qt.KeepAspectRatio,
                Qt.SmoothTransformation,
            )
            self.beam_image_label.setPixmap(scaled)
        else:
            self.beam_image_label.setText("Imagen no disponible")

        self.skyciv_link = QPushButton("🔗 Calcular en SkyCiv")
        self.skyciv_link.setObjectName("skycivLinkButton")
        self.skyciv_link.setCursor(QCursor(Qt.PointingHandCursor))
        self.skyciv_link.setFlat(True)

        beam_layout.addWidget(self.beam_image_label)
        beam_layout.addWidget(self.skyciv_link, alignment=Qt.AlignHCenter)

        return self.beam_card

    def _build_output_panel(self):
        frame = QFrame()
        frame.setObjectName("groupFrame")

        layout = QVBoxLayout(frame)
        layout.setContentsMargins(16, 16, 16, 16)
        layout.setSpacing(12)

        title = QLabel(SECTION_OUTPUTS)
        title.setObjectName("sectionTitle")
        layout.addWidget(title)

        cards_grid = QGridLayout()
        cards_grid.setHorizontalSpacing(12)
        cards_grid.setVerticalSpacing(12)

        self.card_r1 = OutputCard(
            CARD_R1_TITLE,
            INITIAL_R1_VALUE,
            0,
            gauge=True,
            gauge_min=REACTION_GAUGE_MIN,
            gauge_max=REACTION_GAUGE_MAX,
            gauge_value=REACTION_GAUGE_INITIAL,
            gauge_unit=UNIT_FORCE,
            gauge_decimals=1,
            gauge_accent="#4da3ff",
        )

        self.card_r2 = OutputCard(
            CARD_R2_TITLE,
            INITIAL_R2_VALUE,
            0,
            gauge=True,
            gauge_min=REACTION_GAUGE_MIN,
            gauge_max=REACTION_GAUGE_MAX,
            gauge_value=REACTION_GAUGE_INITIAL,
            gauge_unit=UNIT_FORCE,
            gauge_decimals=1,
            gauge_accent="#67d4ff",
        )

        self.card_status = OutputCard(
            CARD_STATUS_TITLE,
            INITIAL_STATUS_VALUE,
            100,
            gauge=False,
        )

        self.card_status.set_percent(33)
        self.card_status.set_progress_color("#808080")

        cards_grid.addWidget(self.card_r1, 0, 0)
        cards_grid.addWidget(self.card_r2, 0, 1)

        if SHOW_FLEX_SECTION:
            self.card_flex = OutputCard(
                CARD_FLEX_TITLE,
                INITIAL_FLEX_VALUE,
                0,
                gauge=True,
                gauge_min=0,
                gauge_max=20,
                gauge_value=0,
                gauge_unit=UNIT_FLEX,
                gauge_decimals=2,
                gauge_accent="#7aa2ff",
            )
            cards_grid.addWidget(self.card_flex, 1, 0)
            cards_grid.addWidget(self.card_status, 1, 1)
        else:
            self.card_flex = None
            cards_grid.addWidget(self.card_status, 1, 0, 1, 2)

        layout.addLayout(cards_grid)
        layout.addStretch()

        return frame

    def _build_footer(self):
        layout = QHBoxLayout()
        layout.setSpacing(10)

        puerto_label = QLabel(LABEL_PORT)
        self.puerto_combo = QComboBox()
        self.puerto_combo.addItems(SERIAL_PORT_OPTIONS)
        if DEFAULT_SERIAL_PORT:
            self.puerto_combo.setCurrentText(DEFAULT_SERIAL_PORT)
        self.puerto_combo.setFixedWidth(120)

        self.btn_actualizar = QPushButton(BUTTON_REFRESH_PORTS)
        self.btn_conectar = QPushButton(BUTTON_CONNECT)
        self.btn_desconectar = QPushButton(BUTTON_DISCONNECT)
        self.btn_desconectar.setEnabled(False)

        self.btn_iniciar = QPushButton(BUTTON_START)
        self.btn_refrescar = QPushButton(BUTTON_REFRESH)

        self.btn_iniciar.setObjectName("primaryButton")
        self.btn_refrescar.setObjectName("successButton")

        self.status_label = QLabel(STATUS_GUI_ONLY)
        self.status_label.setObjectName("statusLabel")

        layout.addWidget(puerto_label)
        layout.addWidget(self.puerto_combo)
        layout.addSpacing(12)
        layout.addWidget(self.btn_actualizar)
        layout.addWidget(self.btn_conectar)
        layout.addWidget(self.btn_desconectar)
        layout.addWidget(self.btn_iniciar)
        layout.addWidget(self.btn_refrescar)
        layout.addStretch()
        layout.addWidget(self.status_label)

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

        if label_text == LABEL_DISTANCE:
            self.distance_input = field

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

        if label_text == LABEL_LOAD:
            self.load_combo = combo

        row.addWidget(label)
        row.addWidget(combo)
        row.addStretch()

        return row