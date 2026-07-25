import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk


class BeamApp(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("Banco de Ensayo - Viga Simplemente Apoyada")
        self.geometry("1100x700")
        self.minsize(900, 600)
        self.configure(bg="#111111")

        self._configure_styles()
        self._build_ui()

    def _configure_styles(self):
        style = ttk.Style(self)

        # En Windows suele andar bien "clam" para personalizar
        style.theme_use("clam")

        style.configure(
            "Dark.TFrame",
            background="#111111"
        )

        style.configure(
            "Panel.TFrame",
            background="#1a1a1a",
            relief="flat"
        )

        style.configure(
            "Dark.TLabel",
            background="#111111",
            foreground="white",
            font=("Segoe UI", 11)
        )

        style.configure(
            "Title.TLabel",
            background="#111111",
            foreground="white",
            font=("Segoe UI", 18, "bold")
        )

        style.configure(
            "Subtitle.TLabel",
            background="#111111",
            foreground="#d9d9d9",
            font=("Segoe UI", 12, "bold")
        )

        style.configure(
            "Status.TLabel",
            background="#111111",
            foreground="#4da6ff",
            font=("Segoe UI", 12, "bold")
        )

        style.configure(
            "Dark.TButton",
            background="#2b2b2b",
            foreground="white",
            borderwidth=1,
            focusthickness=0,
            focuscolor="none",
            padding=(10, 8)
        )

        style.configure(
            "Dark.TButton",
            background="#2b2b2b",
            foreground="white",
            padding=(14, 10),
            borderwidth=0,
            focusthickness=0,
            relief="flat",
            font=("Segoe UI", 10, "bold")
        )

        style.map(
            "Dark.TButton",
            background=[
                ("active", "#3a3a3a"),
                ("pressed", "#1f1f1f")
            ]
        )

        style.configure(
            "Dark.TCombobox",
            fieldbackground="#222222",
            background="#222222",
            foreground="white",
            arrowcolor="white",
            borderwidth=1
        )

    def _build_ui(self):
        root = ttk.Frame(self, style="Dark.TFrame", padding=16)
        root.pack(fill="both", expand=True)

        self._build_header(root)
        self._build_center(root)
        self._build_footer(root)

    def _build_header(self, parent):
        header = ttk.Frame(parent, style="Dark.TFrame")
        header.pack(fill="x", pady=(0, 10))

        left = ttk.Frame(header, style="Dark.TFrame")
        left.pack(side="left", anchor="w")

        img = Image.open("udemm_logo.png")
        img = img.resize((140, 90))
        self.logo_img = ImageTk.PhotoImage(img)

        logo_label = tk.Label(
            left,
            image=self.logo_img,
            bg="#111111"
        )
        logo_label.pack(side="left", padx=(0, 14))

        text_block = ttk.Frame(left, style="Dark.TFrame")
        text_block.pack(side="left", anchor="n")

        ttk.Label(
            text_block,
            text="FACULTAD DE INGENIERÍA - FÍSICA 1",
            style="Subtitle.TLabel"
        ).pack(anchor="w")

        ttk.Label(
            text_block,
            text="VIGA SIMPLEMENTE APOYADA",
            style="Title.TLabel"
        ).pack(anchor="w", pady=(4, 0))

        right = ttk.Frame(header, style="Dark.TFrame")
        right.pack(side="right", anchor="ne")

        ttk.Button(
            right,
            text="GitHub",
            style="Dark.TButton"
        ).pack(side="left", padx=(0, 8))

        ttk.Label(
            right,
            text="v0.1.0",
            style="Dark.TLabel"
        ).pack(side="left", pady=(8, 0))

    def _build_center(self, parent):
        center = ttk.Frame(parent, style="Panel.TFrame")
        center.pack(fill="both", expand=True, pady=(0, 12))

        inner = tk.Frame(
            center,
            bg="#0d0d0d",
            highlightthickness=1,
            highlightbackground="#666666"
        )
        inner.pack(fill="both", expand=True, padx=8, pady=8)

        status = tk.Label(
            inner,
            text="Interfaz lista\n\nAquí luego irá la visualización del ensayo",
            bg="#0d0d0d",
            fg="#4da6ff",
            font=("Segoe UI", 18, "bold"),
            justify="center"
        )
        status.place(relx=0.5, rely=0.45, anchor="center")

    def _build_footer(self, parent):
        footer = ttk.Frame(parent, style="Dark.TFrame")
        footer.pack(fill="x")

        ttk.Label(footer, text="Carga (kg):", style="Dark.TLabel").pack(side="left", padx=(0, 6))

        carga_combo = ttk.Combobox(
            footer,
            values=["0", "1", "2", "5", "10"],
            width=8,
            state="readonly",
            style="Dark.TCombobox"
        )
        carga_combo.set("0")
        carga_combo.pack(side="left", padx=(0, 14))

        ttk.Label(footer, text="Puerto:", style="Dark.TLabel").pack(side="left", padx=(0, 6))

        puerto_combo = ttk.Combobox(
            footer,
            values=["COM3", "COM4", "COM5", "/dev/ttyACM0"],
            width=12,
            state="readonly",
            style="Dark.TCombobox"
        )
        puerto_combo.set("COM3")
        puerto_combo.pack(side="left", padx=(0, 14))

        ttk.Button(footer, text="Actualizar COMs", style="Dark.TButton").pack(side="left", padx=(0, 8))
        ttk.Button(footer, text="Conectar", style="Dark.TButton").pack(side="left", padx=(0, 8))
        ttk.Button(footer, text="Desconectar", style="Dark.TButton").pack(side="left", padx=(0, 8))
        ttk.Button(footer, text="Iniciar", style="Dark.TButton").pack(side="left", padx=(0, 8))
        ttk.Button(footer, text="Detener", style="Dark.TButton").pack(side="left", padx=(0, 8))

        ttk.Label(
            footer,
            text="Estado: GUI sin lógica",
            style="Status.TLabel"
        ).pack(side="right")


if __name__ == "__main__":
    app = BeamApp()
    app.mainloop()