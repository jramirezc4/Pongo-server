"""Cliente TCP minimo para el servidor de Pong."""

import argparse
import socket


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9000
MESSAGE_HELLO = 7
MESSAGE_ACK = 8
HEADER_SIZE = 3


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Cliente TCP de Pong")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Direccion del servidor")
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        type=int,
        help="Puerto del servidor",
    )
    return parser.parse_args()


def receive_all(client_socket: socket.socket, size: int) -> bytes:
    data = bytearray()
    while len(data) < size:
        chunk = client_socket.recv(size - len(data))
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def main() -> int:
    arguments = parse_arguments()

    print(f"Conectando al servidor {arguments.host}:{arguments.port}...")
    try:
        with socket.create_connection((arguments.host, arguments.port)) as client_socket:
            print("Cliente conectado.")
            client_socket.sendall(bytes((MESSAGE_HELLO, 0, 0)))

            response = receive_all(client_socket, HEADER_SIZE)
            if len(response) != HEADER_SIZE:
                print("Respuesta incompleta del servidor.")
                return 1

            message_type = response[0]
            payload_size = int.from_bytes(response[1:3], byteorder="big")
            if message_type != MESSAGE_ACK or payload_size != 0:
                print("Respuesta no reconocida del servidor.")
                return 1

            print("Mensaje HELLO enviado. Respuesta ACK recibida.")
    except ConnectionRefusedError:
        print("No se pudo conectar al servidor.")
        return 1

    print("Cliente desconectado.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
