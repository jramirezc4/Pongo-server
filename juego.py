"""Vista grafica de Pong usando Tkinter."""

import argparse
import socket
import threading
import tkinter as tk
from tkinter import messagebox

from client import (
    MENSAJE_CONFIRMACION,
    MENSAJE_ESTADO_JUEGO,
    MENSAJE_ERROR,
    MENSAJE_MOVER_ABAJO,
    MENSAJE_MOVER_ARRIBA,
    MENSAJE_REGISTRO,
    MENSAJE_SALUDO,
    decodificar_error,
    decodificar_estado_juego,
    enviar_mensaje,
    recibir_mensaje,
)


ANCHO_VENTANA = 800
ALTO_VENTANA = 420
MARGEN = 30
ANCHO_TABLERO_DIBUJO = ANCHO_VENTANA - 2 * MARGEN
ALTO_TABLERO_DIBUJO = ALTO_VENTANA - 2 * MARGEN
ANCHO_CELDA = ANCHO_TABLERO_DIBUJO / 20
ALTO_CELDA = ALTO_TABLERO_DIBUJO / 10
ANCHO_PALETA = 14


def analizar_argumentos() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Vista grafica de Pong")
    parser.add_argument("--host", default="127.0.0.1", help="Direccion del servidor")
    parser.add_argument("--puerto", default=9000, type=int, help="Puerto del servidor")
    parser.add_argument("--apodo", required=True, help="Apodo del jugador")
    parser.add_argument("--correo", required=True, help="Correo del jugador")
    return parser.parse_args()


class VentanaPong:
    def __init__(self, socket_cliente: socket.socket, apodo: str) -> None:
        self.socket_cliente = socket_cliente
        self.apodo = apodo
        self.ventana = tk.Tk()
        self.ventana.title(f"Pong - {apodo}")
        self.ventana.resizable(False, False)
        self.lienzo = tk.Canvas(
            self.ventana,
            width=ANCHO_VENTANA,
            height=ALTO_VENTANA,
            bg="#111827",
            highlightthickness=0,
        )
        self.lienzo.pack()
        self.estado = {
            "paleta_izquierda_y": 3,
            "paleta_derecha_y": 3,
            "pelota_x": 10,
            "pelota_y": 5,
            "pelota_dx": 1,
            "pelota_dy": 1,
            "marcador_izquierdo": 0,
            "marcador_derecho": 0,
        }
        self.conectado = True
        self.ventana.bind("<KeyPress>", self.tecla_presionada)
        self.ventana.protocol("WM_DELETE_WINDOW", self.cerrar)
        self.dibujar()

    def iniciar(self) -> None:
        hilo = threading.Thread(target=self.recibir_estados, daemon=True)
        hilo.start()
        self.ventana.focus_force()
        self.ventana.mainloop()

    def tecla_presionada(self, evento: tk.Event) -> None:
        if evento.keysym in ("w", "W", "Up"):
            enviar_mensaje(self.socket_cliente, MENSAJE_MOVER_ARRIBA)
        elif evento.keysym in ("s", "S", "Down"):
            enviar_mensaje(self.socket_cliente, MENSAJE_MOVER_ABAJO)
        elif evento.keysym in ("q", "Q", "Escape"):
            self.cerrar()

    def recibir_estados(self) -> None:
        try:
            while self.conectado:
                tipo_mensaje, carga_util = recibir_mensaje(self.socket_cliente)
                if tipo_mensaje == MENSAJE_ESTADO_JUEGO:
                    nuevo_estado = decodificar_estado_juego(carga_util)
                    self.ventana.after(0, self.actualizar_estado, nuevo_estado)
                elif tipo_mensaje == MENSAJE_ERROR:
                    raise ConnectionError(decodificar_error(carga_util))
        except (ConnectionError, OSError) as exc:
            if self.conectado:
                self.ventana.after(0, self.mostrar_error, str(exc))

    def actualizar_estado(self, estado: dict[str, int]) -> None:
        self.estado = estado
        self.dibujar()

    def dibujar(self) -> None:
        self.lienzo.delete("all")
        self.lienzo.create_rectangle(
            MARGEN,
            MARGEN,
            ANCHO_VENTANA - MARGEN,
            ALTO_VENTANA - MARGEN,
            outline="#374151",
            width=2,
        )
        self.lienzo.create_text(
            ANCHO_VENTANA / 2,
            18,
            text=(
                f"{self.estado['marcador_izquierdo']}   -   "
                f"{self.estado['marcador_derecho']}"
            ),
            fill="white",
            font=("Arial", 18, "bold"),
        )
        self.lienzo.create_line(
            ANCHO_VENTANA / 2,
            MARGEN,
            ANCHO_VENTANA / 2,
            ALTO_VENTANA - MARGEN,
            fill="#4b5563",
            dash=(5, 5),
        )
        self.dibujar_paleta(self.estado["paleta_izquierda_y"], MARGEN + 8)
        self.dibujar_paleta(
            self.estado["paleta_derecha_y"],
            ANCHO_VENTANA - MARGEN - 8 - ANCHO_PALETA,
        )
        pelota_x = MARGEN + self.estado["pelota_x"] * ANCHO_CELDA
        pelota_y = MARGEN + self.estado["pelota_y"] * ALTO_CELDA
        radio = 8
        self.lienzo.create_oval(
            pelota_x - radio,
            pelota_y - radio,
            pelota_x + radio,
            pelota_y + radio,
            fill="#fbbf24",
            outline="",
        )
        self.lienzo.create_text(
            ANCHO_VENTANA / 2,
            ALTO_VENTANA - 12,
            text="W/S o flechas: mover    Q o Esc: salir",
            fill="#d1d5db",
            font=("Arial", 10),
        )

    def dibujar_paleta(self, posicion_y: int, posicion_x: float) -> None:
        inicio_y = MARGEN + posicion_y * ALTO_CELDA
        fin_y = inicio_y + 4 * ALTO_CELDA
        self.lienzo.create_rectangle(
            posicion_x,
            inicio_y,
            posicion_x + ANCHO_PALETA,
            fin_y,
            fill="#60a5fa",
            outline="",
        )

    def mostrar_error(self, mensaje: str) -> None:
        messagebox.showerror("Error de conexion", mensaje)
        self.cerrar()

    def cerrar(self) -> None:
        if self.conectado:
            self.conectado = False
            self.socket_cliente.close()
        self.ventana.destroy()


def conectar(host: str, puerto: int, apodo: str, correo: str) -> socket.socket:
    socket_cliente = socket.create_connection((host, puerto))
    try:
        enviar_mensaje(socket_cliente, MENSAJE_SALUDO)
        tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)
        if tipo_mensaje != MENSAJE_CONFIRMACION or carga_util:
            raise ConnectionError("El servidor no confirmo el saludo.")
        enviar_mensaje(
            socket_cliente,
            MENSAJE_REGISTRO,
            f"{apodo}\0{correo}\0".encode("utf-8"),
        )
        tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)
        if tipo_mensaje == MENSAJE_ERROR:
            raise ConnectionError(decodificar_error(carga_util))
        if tipo_mensaje != MENSAJE_CONFIRMACION:
            raise ConnectionError("El servidor no confirmo el registro.")
        return socket_cliente
    except (ConnectionError, OSError):
        socket_cliente.close()
        raise


def main() -> int:
    argumentos = analizar_argumentos()
    try:
        socket_cliente = conectar(
            argumentos.host,
            argumentos.puerto,
            argumentos.apodo,
            argumentos.correo,
        )
    except (ConnectionError, OSError) as exc:
        print(f"No se pudo iniciar la partida: {exc}")
        return 1

    VentanaPong(socket_cliente, argumentos.apodo).iniciar()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
