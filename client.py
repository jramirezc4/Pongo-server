"""Cliente TCP sencillo para probar una partida de Pong."""

import argparse
import os
import socket
import time


SERVIDOR_POR_DEFECTO = "127.0.0.1"
PUERTO_POR_DEFECTO = 9000
MENSAJE_REGISTRO = 1
MENSAJE_MOVER_ARRIBA = 2
MENSAJE_MOVER_ABAJO = 3
MENSAJE_ESTADO_JUEGO = 4
MENSAJE_ERROR = 6
MENSAJE_SALUDO = 7
MENSAJE_CONFIRMACION = 8
TAMANO_ENCABEZADO = 3
TAMANO_ESTADO_JUEGO = 8


def analizar_argumentos() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Cliente TCP de Pong")
    parser.add_argument("--host", default=SERVIDOR_POR_DEFECTO, help="Direccion del servidor")
    parser.add_argument("--puerto", default=PUERTO_POR_DEFECTO, type=int, help="Puerto")
    parser.add_argument("--apodo", default=None, help="apodo del jugador")
    parser.add_argument("--correo", default=None, help="Correo del jugador")
    parser.add_argument(
        "--mover",
        choices=("arriba", "abajo"),
        default=None,
        help="Enviar un movimiento despues de registrarse",
    )
    parser.add_argument(
        "--segundos",
        default=3,
        type=float,
        help="Segundos para observar estados del juego",
    )
    parser.add_argument(
        "--jugar",
        action="store_true",
        help="Activar el modo interactivo con teclado",
    )
    return parser.parse_args()


def recibir_todo(socket_cliente: socket.socket, tamano: int) -> bytes:
    datos = bytearray()
    while len(datos) < tamano:
        fragmento = socket_cliente.recv(tamano - len(datos))
        if not fragmento:
            break
        datos.extend(fragmento)
    return bytes(datos)


def enviar_mensaje(
    socket_cliente: socket.socket, tipo_mensaje: int, carga_util: bytes = b""
) -> None:
    if len(carga_util) > 65535:
        raise ValueError("La carga util es demasiado grande.")
    encabezado = bytes((tipo_mensaje, (len(carga_util) >> 8) & 0xFF, len(carga_util) & 0xFF))
    socket_cliente.sendall(encabezado + carga_util)


def recibir_mensaje(socket_cliente: socket.socket) -> tuple[int, bytes]:
    encabezado = recibir_todo(socket_cliente, TAMANO_ENCABEZADO)
    if len(encabezado) != TAMANO_ENCABEZADO:
        raise ConnectionError("Respuesta incompleta del servidor.")

    tamano_carga = int.from_bytes(encabezado[1:3], byteorder="big")
    carga_util = recibir_todo(socket_cliente, tamano_carga)
    if len(carga_util) != tamano_carga:
        raise ConnectionError("La carga util llego incompleta.")
    return encabezado[0], carga_util


def decodificar_estado_juego(carga_util: bytes) -> dict[str, int]:
    if len(carga_util) != TAMANO_ESTADO_JUEGO:
        raise ValueError("El estado del juego tiene un tamanio inesperado.")

    def con_signo(valor: int) -> int:
        return valor - 256 if valor > 127 else valor

    return {
        "paleta_izquierda_y": carga_util[0],
        "paleta_derecha_y": carga_util[1],
        "pelota_x": carga_util[2],
        "pelota_y": carga_util[3],
        "pelota_dx": con_signo(carga_util[4]),
        "pelota_dy": con_signo(carga_util[5]),
        "marcador_izquierdo": carga_util[6],
        "marcador_derecho": carga_util[7],
    }


def leer_datos_jugador(argumentos: argparse.Namespace) -> tuple[str, str]:
    apodo = argumentos.apodo
    correo = argumentos.correo
    if apodo is None:
        apodo = input("apodo: ").strip()
    if correo is None:
        correo = input("Correo electronico: ").strip()
    return apodo, correo


def mostrar_estado(estado: dict[str, int]) -> None:
    print(
        "Estado: "
        f"paletas ({estado['paleta_izquierda_y']}, {estado['paleta_derecha_y']}) "
        f"pelota ({estado['pelota_x']}, {estado['pelota_y']}) "
        f"velocidad ({estado['pelota_dx']}, {estado['pelota_dy']}) "
        f"marcador {estado['marcador_izquierdo']}-{estado['marcador_derecho']}"
    )


def decodificar_error(carga_util: bytes) -> str:
    return carga_util.rstrip(b"\0").decode(errors="replace")


def enviar_movimiento(socket_cliente: socket.socket, tecla: str) -> bool:
    if tecla in ("w", "W", "arriba"):
        enviar_mensaje(socket_cliente, MENSAJE_MOVER_ARRIBA)
        return True
    if tecla in ("s", "S", "abajo"):
        enviar_mensaje(socket_cliente, MENSAJE_MOVER_ABAJO)
        return True
    return False


def jugar_partida(socket_cliente: socket.socket) -> None:
    import msvcrt

    print("Partida iniciada. Usa W/S o las flechas para mover. Pulsa Q para salir.")
    socket_cliente.settimeout(0.1)
    jugando = True
    while jugando:
        while msvcrt.kbhit():
            tecla = msvcrt.getwch()
            if tecla in ("\x00", "\xe0"):
                tecla = msvcrt.getwch()
                if tecla == "H":
                    enviar_movimiento(socket_cliente, "arriba")
                elif tecla == "P":
                    enviar_movimiento(socket_cliente, "abajo")
            elif tecla in ("q", "Q"):
                jugando = False
                break
            else:
                enviar_movimiento(socket_cliente, tecla)

        if not jugando:
            break

        try:
            tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)
        except socket.timeout:
            continue

        if tipo_mensaje == MENSAJE_ESTADO_JUEGO:
            os.system("cls")
            mostrar_estado(decodificar_estado_juego(carga_util))
            print("\nControles: W/S o flechas | Q: salir")
        elif tipo_mensaje == MENSAJE_ERROR:
            raise ConnectionError(
                f"Error del servidor: {decodificar_error(carga_util)}"
            )


def main() -> int:
    argumentos = analizar_argumentos()
    apodo = ""

    print(f"Conectando al servidor {argumentos.host}:{argumentos.puerto}...")
    try:
        with socket.create_connection((argumentos.host, argumentos.puerto)) as socket_cliente:
            print("Cliente conectado.")
            enviar_mensaje(socket_cliente, MENSAJE_SALUDO)
            tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)
            if tipo_mensaje != MENSAJE_CONFIRMACION or carga_util:
                print("Respuesta de saludo no reconocida.")
                return 1

            apodo, correo = leer_datos_jugador(argumentos)
            if not apodo:
                print("El apodo no puede estar vacio.")
                return 1

            enviar_mensaje(
                socket_cliente,
                MENSAJE_REGISTRO,
                f"{apodo}\0{correo}\0".encode("utf-8"),
            )
            tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)
            if tipo_mensaje == MENSAJE_ERROR:
                print(f"Registro rechazado: {decodificar_error(carga_util)}")
                return 1
            if tipo_mensaje != MENSAJE_CONFIRMACION:
                print("Respuesta inesperada durante el registro.")
                return 1
            print(f"Jugador {apodo} confirmado.")

            if argumentos.jugar:
                jugar_partida(socket_cliente)
            else:
                socket_cliente.settimeout(0.5)
                limite_tiempo = time.monotonic() + max(argumentos.segundos, 0)
                movimiento_enviado = False
                while time.monotonic() < limite_tiempo:
                    if argumentos.mover and not movimiento_enviado:
                        enviar_movimiento(socket_cliente, argumentos.mover)
                        movimiento_enviado = True
                    try:
                        tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)
                    except socket.timeout:
                        continue
                    if tipo_mensaje == MENSAJE_ESTADO_JUEGO:
                        mostrar_estado(decodificar_estado_juego(carga_util))
                    elif tipo_mensaje == MENSAJE_ERROR:
                        print(f"Error del servidor: {decodificar_error(carga_util)}")
                        return 1
    except (ConnectionError, OSError, ValueError) as exc:
        print(f"Error: {exc}")
        return 1

    print("Cliente desconectado.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
