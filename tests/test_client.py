import unittest

from client import decode_game_state, receive_message, send_message


class FakeSocket:
    def __init__(self, chunks=()):
        self.chunks = list(chunks)
        self.sent = []

    def sendall(self, data):
        self.sent.append(data)

    def recv(self, size):
        if not self.chunks:
            return b""
        return self.chunks.pop(0)[:size]


class ClientProtocolTests(unittest.TestCase):
    def test_send_message_builds_binary_header(self):
        client_socket = FakeSocket()

        send_message(client_socket, 4, b"abc")

        self.assertEqual(client_socket.sent, [b"\x04\x00\x03abc"])

    def test_receive_message_reads_header_and_payload(self):
        client_socket = FakeSocket((b"\x04\x00\x03", b"abc"))

        message_type, payload = receive_message(client_socket)

        self.assertEqual(message_type, 4)
        self.assertEqual(payload, b"abc")

    def test_decode_game_state_reads_scores_and_signed_velocity(self):
        state = decode_game_state(b"\x03\x04\x0a\x05\xff\x01\x02\x07")

        self.assertEqual(state["left_paddle_y"], 3)
        self.assertEqual(state["right_paddle_y"], 4)
        self.assertEqual(state["ball_dx"], -1)
        self.assertEqual(state["ball_dy"], 1)
        self.assertEqual(state["left_score"], 2)
        self.assertEqual(state["right_score"], 7)


if __name__ == "__main__":
    unittest.main()
