"""Cliente TCP sencillo para probar una partida de Pong."""

import argparse
import socket
import time


DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9000
MESSAGE_REGISTER = 1
MESSAGE_MOVE_UP = 2
MESSAGE_MOVE_DOWN = 3
MESSAGE_GAME_STATE = 4
MESSAGE_ERROR = 6
MESSAGE_HELLO = 7
MESSAGE_ACK = 8
HEADER_SIZE = 3
GAME_STATE_SIZE = 8


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Cliente TCP de Pong")
    parser.add_argument("--host", default=DEFAULT_HOST, help="Direccion del servidor")
    parser.add_argument("--port", default=DEFAULT_PORT, type=int, help="Puerto")
    parser.add_argument("--nickname", default=None, help="Nickname del jugador")
    parser.add_argument("--email", default=None, help="Correo del jugador")
    parser.add_argument(
        "--move",
        choices=("up", "down"),
        default=None,
        help="Enviar un movimiento despues de registrarse",
    )
    parser.add_argument(
        "--seconds",
        default=3,
        type=float,
        help="Segundos para observar estados del juego",
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


def send_message(
    client_socket: socket.socket, message_type: int, payload: bytes = b""
) -> None:
    if len(payload) > 65535:
        raise ValueError("La carga util es demasiado grande.")
    header = bytes((message_type, (len(payload) >> 8) & 0xFF, len(payload) & 0xFF))
    client_socket.sendall(header + payload)


def receive_message(client_socket: socket.socket) -> tuple[int, bytes]:
    header = receive_all(client_socket, HEADER_SIZE)
    if len(header) != HEADER_SIZE:
        raise ConnectionError("Respuesta incompleta del servidor.")

    payload_size = int.from_bytes(header[1:3], byteorder="big")
    payload = receive_all(client_socket, payload_size)
    if len(payload) != payload_size:
        raise ConnectionError("La carga util llego incompleta.")
    return header[0], payload


def decode_game_state(payload: bytes) -> dict[str, int]:
    if len(payload) != GAME_STATE_SIZE:
        raise ValueError("El estado del juego tiene un tamanio inesperado.")

    def signed(value: int) -> int:
        return value - 256 if value > 127 else value

    return {
        "left_paddle_y": payload[0],
        "right_paddle_y": payload[1],
        "ball_x": payload[2],
        "ball_y": payload[3],
        "ball_dx": signed(payload[4]),
        "ball_dy": signed(payload[5]),
        "left_score": payload[6],
        "right_score": payload[7],
    }


def read_credentials(arguments: argparse.Namespace) -> tuple[str, str]:
    nickname = arguments.nickname
    email = arguments.email
    if nickname is None:
        nickname = input("Nickname: ").strip()
    if email is None:
        email = input("Correo electronico: ").strip()
    return nickname, email


def print_state(state: dict[str, int]) -> None:
    print(
        "Estado: "
        f"paletas ({state['left_paddle_y']}, {state['right_paddle_y']}) "
        f"pelota ({state['ball_x']}, {state['ball_y']}) "
        f"velocidad ({state['ball_dx']}, {state['ball_dy']}) "
        f"marcador {state['left_score']}-{state['right_score']}"
    )


def decode_error(payload: bytes) -> str:
    return payload.rstrip(b"\0").decode(errors="replace")


def main() -> int:
    arguments = parse_arguments()
    nickname = ""

    print(f"Conectando al servidor {arguments.host}:{arguments.port}...")
    try:
        with socket.create_connection((arguments.host, arguments.port)) as client_socket:
            print("Cliente conectado.")
            send_message(client_socket, MESSAGE_HELLO)
            message_type, payload = receive_message(client_socket)
            if message_type != MESSAGE_ACK or payload:
                print("Respuesta HELLO no reconocida.")
                return 1

            nickname, email = read_credentials(arguments)
            if not nickname:
                print("El nickname no puede estar vacio.")
                return 1

            send_message(
                client_socket,
                MESSAGE_REGISTER,
                f"{nickname}\0{email}\0".encode("utf-8"),
            )
            message_type, payload = receive_message(client_socket)
            if message_type == MESSAGE_ERROR:
                print(f"Registro rechazado: {decode_error(payload)}")
                return 1
            if message_type != MESSAGE_ACK:
                print("Respuesta inesperada durante el registro.")
                return 1
            print(f"Jugador {nickname} confirmado.")

            client_socket.settimeout(0.5)
            deadline = time.monotonic() + max(arguments.seconds, 0)
            movement_sent = False
            while time.monotonic() < deadline:
                if arguments.move and not movement_sent:
                    movement_type = (
                        MESSAGE_MOVE_UP if arguments.move == "up" else MESSAGE_MOVE_DOWN
                    )
                    send_message(client_socket, movement_type)
                    movement_sent = True
                try:
                    message_type, payload = receive_message(client_socket)
                except socket.timeout:
                    continue
                if message_type == MESSAGE_GAME_STATE:
                    print_state(decode_game_state(payload))
                elif message_type == MESSAGE_ERROR:
                    print(f"Error del servidor: {decode_error(payload)}")
                    return 1
    except (ConnectionError, OSError, ValueError) as exc:
        print(f"Error: {exc}")
        return 1

    print("Cliente desconectado.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
