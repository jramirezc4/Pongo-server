"""Cliente TCP para el servidor de Pong."""

import argparse
import socket


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9000
MESSAGE_REGISTER = 1
MESSAGE_GAME_STATE = 4
MESSAGE_ERROR = 6
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


def send_message(client_socket: socket.socket, message_type: int, payload: bytes = b"") -> None:
    header = bytes((message_type, (len(payload) >> 8) & 0xFF, len(payload) & 0xFF))
    client_socket.sendall(header + payload)


def receive_message(client_socket: socket.socket) -> tuple[int, bytes]:
    header = receive_all(client_socket, HEADER_SIZE)
    if len(header) != HEADER_SIZE:
        raise ConnectionError("Respuesta incompleta del servidor.")

    message_type = header[0]
    payload_size = int.from_bytes(header[1:3], byteorder="big")
    payload = receive_all(client_socket, payload_size)
    if len(payload) != payload_size:
        raise ConnectionError("La carga util del mensaje llego incompleta.")

    return message_type, payload


def decode_game_state(payload: bytes) -> dict[str, int]:
    if len(payload) != 6:
        raise ValueError("La carga del estado del juego tiene un tamanio inesperado.")

    return {
        "left_paddle_y": payload[0],
        "right_paddle_y": payload[1],
        "ball_x": payload[2],
        "ball_y": payload[3],
        "ball_dx": payload[4],
        "ball_dy": payload[5],
    }


def main() -> int:
    arguments = parse_arguments()

    print(f"Conectando al servidor {arguments.host}:{arguments.port}...")
    try:
        with socket.create_connection((arguments.host, arguments.port)) as client_socket:
            print("Cliente conectado.")
            send_message(client_socket, MESSAGE_HELLO)

            message_type, payload = receive_message(client_socket)
            if message_type != MESSAGE_ACK or payload:
                print("Respuesta no reconocida del servidor.")
                return 1
            print("Mensaje HELLO enviado. Respuesta ACK recibida.")

            nickname = input("Nickname: ").strip()
            email = input("Correo electronico: ").strip()
            if not nickname:
                print("El nickname no puede estar vacio.")
                return 1

            registration_payload = f"{nickname}\0{email}\0".encode("utf-8")
            send_message(client_socket, MESSAGE_REGISTER, registration_payload)

            message_type, payload = receive_message(client_socket)
            if message_type == MESSAGE_ERROR:
                print(f"Registro rechazado: {payload.decode('utf-8', errors='replace').rstrip(chr(0))}")
                return 1
            if message_type != MESSAGE_ACK:
                print("Respuesta inesperada del servidor durante el registro.")
                return 1

            print("Registro recibido por el servidor.")

            message_type, payload = receive_message(client_socket)
            if message_type != MESSAGE_GAME_STATE:
                print("El servidor no envio el estado inicial del juego.")
                return 1

            state = decode_game_state(payload)
            print(
                "Estado inicial del juego: "
                f"paletas ({state['left_paddle_y']}, {state['right_paddle_y']}) "
                f"pelota ({state['ball_x']}, {state['ball_y']}) "
                f"velocidad ({state['ball_dx']}, {state['ball_dy']})"
            )
    except ConnectionRefusedError:
        print("No se pudo conectar al servidor.")
        return 1
    except (ConnectionError, ValueError) as exc:
        print(f"Error: {exc}")
        return 1

    print("Cliente desconectado.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
