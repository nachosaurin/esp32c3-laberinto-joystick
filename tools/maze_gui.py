import argparse
import queue
import re
import threading
import time
import tkinter as tk
from tkinter import messagebox
from tkinter import ttk

import serial
from serial.tools import list_ports


BAUDRATE = 115200
MAZE_WIDTH = 10
MAZE_HEIGHT = 8
CELL_SIZE = 52

DEFAULT_MAZE = [
    "##########",
    "#P..#...G#",
    "#.#.#.##.#",
    "#.#....#.#",
    "#.####.#.#",
    "#....#...#",
    "####...#.#",
    "##########",
]

ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
POSITION_RE = re.compile(r"Posicion:\s*x=(\d+),\s*y=(\d+)\s*\|\s*Movimientos validos:\s*(\d+)")
ROW_WITH_AXIS_RE = re.compile(r"^y=(\d+)\s+([#PG.]{%d})$" % MAZE_WIDTH)
BARE_ROW_RE = re.compile(r"^[#PG.]{%d}$" % MAZE_WIDTH)
COMMAND_RE = re.compile(r"Comando recibido:\s*([WASDRwasdr])")


class SerialWorker(threading.Thread):
    def __init__(self, port, output_queue):
        super().__init__(daemon=True)
        self.port = port
        self.output_queue = output_queue
        self.stop_event = threading.Event()
        self.serial_port = None

    def run(self):
        try:
            self.serial_port = serial.Serial(self.port, BAUDRATE, timeout=0.1, write_timeout=0.2)
            self.output_queue.put(("connected", self.port))
        except serial.SerialException as exc:
            self.output_queue.put(("error", f"No se pudo abrir {self.port}: {exc}"))
            return

        buffer = bytearray()
        while not self.stop_event.is_set():
            try:
                data = self.serial_port.read(128)
            except serial.SerialException as exc:
                self.output_queue.put(("error", f"Error leyendo puerto serie: {exc}"))
                break

            if not data:
                continue

            buffer.extend(data)
            while b"\n" in buffer:
                line, _, buffer = buffer.partition(b"\n")
                text = line.decode("utf-8", errors="replace").strip("\r")
                self.output_queue.put(("line", text))

        try:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
        finally:
            self.output_queue.put(("disconnected", None))

    def write_command(self, command):
        if self.serial_port is None or not self.serial_port.is_open:
            return False

        try:
            self.serial_port.write((command + "\n").encode("ascii"))
            return True
        except serial.SerialException as exc:
            self.output_queue.put(("error", f"Error enviando comando: {exc}"))
            return False

    def stop(self):
        self.stop_event.set()


class MazeGui(tk.Tk):
    def __init__(self, default_port):
        super().__init__()
        self.title("Proyecto 24 - Laberinto ESP32-C3")
        self.geometry("940x590")
        self.minsize(860, 540)

        self.event_queue = queue.Queue()
        self.worker = None
        self.pending_rows = {}
        self.maze_rows = list(DEFAULT_MAZE)
        self.moves = 0
        self.position = (1, 1)
        self.last_command = "-"
        self.game_status = "Desconectado"
        self.default_port = default_port

        self._build_style()
        self._build_layout()
        self._draw_maze()
        self._bind_keys()
        self.after(80, self._process_events)

    def _build_style(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TFrame", background="#1f2329")
        style.configure("Panel.TFrame", background="#2a3038")
        style.configure("TLabel", background="#1f2329", foreground="#f4f7fb", font=("Segoe UI", 10))
        style.configure("Panel.TLabel", background="#2a3038", foreground="#f4f7fb", font=("Segoe UI", 10))
        style.configure("Title.TLabel", background="#1f2329", foreground="#ffffff", font=("Segoe UI", 18, "bold"))
        style.configure("Metric.TLabel", background="#2a3038", foreground="#ffffff", font=("Segoe UI", 13, "bold"))
        style.configure("TButton", font=("Segoe UI", 10, "bold"), padding=8)
        style.configure("Move.TButton", font=("Segoe UI", 16, "bold"), padding=10)
        style.configure("TCombobox", padding=4)

    def _build_layout(self):
        main = ttk.Frame(self, padding=16)
        main.pack(fill="both", expand=True)

        header = ttk.Frame(main)
        header.pack(fill="x", pady=(0, 14))

        title = ttk.Label(header, text="Laberinto ESP32-C3", style="Title.TLabel")
        title.pack(side="left")

        connection = ttk.Frame(header)
        connection.pack(side="right")

        self.port_var = tk.StringVar(value=self.default_port)
        self.port_combo = ttk.Combobox(connection, textvariable=self.port_var, width=12, values=self._serial_ports())
        self.port_combo.pack(side="left", padx=(0, 8))

        self.connect_button = ttk.Button(connection, text="Conectar", command=self._connect)
        self.connect_button.pack(side="left", padx=(0, 8))

        self.disconnect_button = ttk.Button(connection, text="Desconectar", command=self._disconnect, state="disabled")
        self.disconnect_button.pack(side="left")

        body = ttk.Frame(main)
        body.pack(fill="both", expand=True)

        board_panel = ttk.Frame(body, style="Panel.TFrame", padding=12)
        board_panel.pack(side="left", fill="both", expand=False)

        self.canvas = tk.Canvas(
            board_panel,
            width=MAZE_WIDTH * CELL_SIZE,
            height=MAZE_HEIGHT * CELL_SIZE,
            background="#15181d",
            highlightthickness=0,
        )
        self.canvas.pack()

        side = ttk.Frame(body, style="Panel.TFrame", padding=14)
        side.pack(side="left", fill="both", expand=True, padx=(16, 0))

        self.status_label = ttk.Label(side, text="Estado: Desconectado", style="Metric.TLabel")
        self.status_label.pack(anchor="w", pady=(0, 8))

        self.moves_label = ttk.Label(side, text="Movimientos: 0", style="Panel.TLabel")
        self.moves_label.pack(anchor="w", pady=2)

        self.position_label = ttk.Label(side, text="Posicion: x=1, y=1", style="Panel.TLabel")
        self.position_label.pack(anchor="w", pady=2)

        self.command_label = ttk.Label(side, text="Ultimo comando: -", style="Panel.TLabel")
        self.command_label.pack(anchor="w", pady=(2, 14))

        controls = ttk.Frame(side, style="Panel.TFrame")
        controls.pack(anchor="w", pady=(4, 16))

        ttk.Button(controls, text="W", style="Move.TButton", width=6, command=lambda: self._send("W")).grid(row=0, column=1, padx=4, pady=4)
        ttk.Button(controls, text="A", style="Move.TButton", width=6, command=lambda: self._send("A")).grid(row=1, column=0, padx=4, pady=4)
        ttk.Button(controls, text="S", style="Move.TButton", width=6, command=lambda: self._send("S")).grid(row=1, column=1, padx=4, pady=4)
        ttk.Button(controls, text="D", style="Move.TButton", width=6, command=lambda: self._send("D")).grid(row=1, column=2, padx=4, pady=4)

        reset_button = ttk.Button(side, text="Reiniciar partida (R)", command=lambda: self._send("R"))
        reset_button.pack(anchor="w", fill="x", pady=(0, 14))

        legend = ttk.Label(
            side,
            text="Leyenda: P jugador | # pared | . camino | G meta",
            style="Panel.TLabel",
        )
        legend.pack(anchor="w", pady=(0, 10))

        log_label = ttk.Label(side, text="Mensajes de la ESP", style="Panel.TLabel")
        log_label.pack(anchor="w")

        self.log = tk.Text(side, height=10, bg="#171b20", fg="#d7dde7", insertbackground="#ffffff", relief="flat")
        self.log.pack(fill="both", expand=True, pady=(4, 0))

    def _serial_ports(self):
        ports = [port.device for port in list_ports.comports()]
        if self.default_port not in ports:
            ports.insert(0, self.default_port)
        return ports

    def _bind_keys(self):
        self.bind("<KeyPress-w>", lambda _event: self._send("W"))
        self.bind("<KeyPress-a>", lambda _event: self._send("A"))
        self.bind("<KeyPress-s>", lambda _event: self._send("S"))
        self.bind("<KeyPress-d>", lambda _event: self._send("D"))
        self.bind("<KeyPress-r>", lambda _event: self._send("R"))
        self.protocol("WM_DELETE_WINDOW", self._on_close)

    def _connect(self):
        if self.worker is not None:
            return

        port = self.port_var.get().strip() or self.default_port
        self._set_status(f"Conectando a {port}...")
        self.worker = SerialWorker(port, self.event_queue)
        self.worker.start()
        self.connect_button.configure(state="disabled")
        self.disconnect_button.configure(state="normal")

    def _disconnect(self):
        if self.worker is not None:
            self.worker.stop()
            self.worker = None
        self.connect_button.configure(state="normal")
        self.disconnect_button.configure(state="disabled")
        self._set_status("Desconectado")

    def _send(self, command):
        self.last_command = command
        self._update_metrics()

        if self.worker is None:
            self._set_status("Conecta COM7 antes de enviar comandos")
            return

        sent = self.worker.write_command(command)
        if sent:
            self._append_log(f"> {command}")
        else:
            self._set_status("No se pudo enviar el comando")

    def _process_events(self):
        while True:
            try:
                event_type, payload = self.event_queue.get_nowait()
            except queue.Empty:
                break

            if event_type == "connected":
                self._set_status(f"Conectado a {payload}")
                self._append_log(f"Conectado a {payload}")
            elif event_type == "disconnected":
                self.worker = None
                self.connect_button.configure(state="normal")
                self.disconnect_button.configure(state="disabled")
                self._set_status("Desconectado")
            elif event_type == "error":
                self._set_status("Error")
                self._append_log(payload)
                messagebox.showerror("Puerto serie", payload)
            elif event_type == "line":
                self._handle_line(payload)

        self.after(80, self._process_events)

    def _handle_line(self, line):
        line = ANSI_RE.sub("", line).strip()
        if not line:
            return

        self._append_log(line)

        position_match = POSITION_RE.search(line)
        if position_match:
            self.position = (int(position_match.group(1)), int(position_match.group(2)))
            self.moves = int(position_match.group(3))
            self._set_status("Jugando")
            self._update_metrics()
            return

        command_match = COMMAND_RE.search(line)
        if command_match:
            self.last_command = command_match.group(1).upper()
            self._update_metrics()
            return

        row_match = ROW_WITH_AXIS_RE.match(line)
        if row_match:
            row_index = int(row_match.group(1))
            self.pending_rows[row_index] = row_match.group(2)
            self._commit_rows_if_ready()
            return

        if BARE_ROW_RE.match(line):
            next_index = len(self.pending_rows)
            if next_index < MAZE_HEIGHT:
                self.pending_rows[next_index] = line
                self._commit_rows_if_ready()
            return

        if "Movimiento bloqueado" in line:
            self._set_status("Movimiento bloqueado")
            return

        if "Laberinto completado" in line:
            self._set_status("Laberinto completado")
            return

        if "Partida reiniciada" in line:
            self._set_status("Partida reiniciada")
            return

    def _commit_rows_if_ready(self):
        if len(self.pending_rows) < MAZE_HEIGHT:
            return

        rows = []
        for index in range(MAZE_HEIGHT):
            row = self.pending_rows.get(index)
            if row is None:
                return
            rows.append(row)

        self.maze_rows = rows
        self.pending_rows.clear()
        self._draw_maze()

    def _draw_maze(self):
        self.canvas.delete("all")
        colors = {
            "#": ("#303844", "#46515f"),
            ".": ("#f1f3f5", "#d5dbe3"),
            "P": ("#2ecc71", "#27ae60"),
            "G": ("#f1c40f", "#d4a90e"),
        }

        for y, row in enumerate(self.maze_rows):
            for x, cell in enumerate(row):
                x0 = x * CELL_SIZE
                y0 = y * CELL_SIZE
                x1 = x0 + CELL_SIZE
                y1 = y0 + CELL_SIZE
                fill, outline = colors.get(cell, colors["."])
                self.canvas.create_rectangle(x0, y0, x1, y1, fill=fill, outline=outline, width=2)

                if cell == "P":
                    self.canvas.create_oval(x0 + 10, y0 + 10, x1 - 10, y1 - 10, fill="#145a32", outline="")
                    self.canvas.create_text((x0 + x1) / 2, (y0 + y1) / 2, text="P", fill="#ffffff", font=("Segoe UI", 18, "bold"))
                elif cell == "G":
                    self.canvas.create_text((x0 + x1) / 2, (y0 + y1) / 2, text="G", fill="#3a2d00", font=("Segoe UI", 18, "bold"))
                elif cell == "#":
                    self.canvas.create_text((x0 + x1) / 2, (y0 + y1) / 2, text="#", fill="#d0d6de", font=("Consolas", 16, "bold"))

    def _set_status(self, status):
        self.game_status = status
        self._update_metrics()

    def _update_metrics(self):
        self.status_label.configure(text=f"Estado: {self.game_status}")
        self.moves_label.configure(text=f"Movimientos: {self.moves}")
        self.position_label.configure(text=f"Posicion: x={self.position[0]}, y={self.position[1]}")
        self.command_label.configure(text=f"Ultimo comando: {self.last_command}")

    def _append_log(self, text):
        self.log.insert("end", text + "\n")
        self.log.see("end")

    def _on_close(self):
        self._disconnect()
        time.sleep(0.1)
        self.destroy()


def main():
    parser = argparse.ArgumentParser(description="Interfaz grafica para Proyecto 24 - ESP32-C3")
    parser.add_argument("--port", default="COM7", help="Puerto serie de la ESP32-C3")
    args = parser.parse_args()

    app = MazeGui(args.port)
    app.mainloop()


if __name__ == "__main__":
    main()
