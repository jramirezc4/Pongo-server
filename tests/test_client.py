import unittest

from client import decodificar_estado_juego, recibir_mensaje, enviar_mensaje


class SocketFalso:
    def __init__(self, chunks=()):
        self.chunks = list(chunks)
        self.sent = []

    def sendall(self, datos):
        self.sent.append(datos)

    def recv(self, tamano):
        if not self.chunks:
            return b""
        return self.chunks.pop(0)[:tamano]


class PruebasProtocoloCliente(unittest.TestCase):
    def test_enviar_mensaje_construye_encabezado_binario(self):
        socket_cliente = SocketFalso()

        enviar_mensaje(socket_cliente, 4, b"abc")

        self.assertEqual(socket_cliente.sent, [b"\x04\x00\x03abc"])

    def test_recibir_mensaje_lee_encabezado_y_carga(self):
        socket_cliente = SocketFalso((b"\x04\x00\x03", b"abc"))

        tipo_mensaje, carga_util = recibir_mensaje(socket_cliente)

        self.assertEqual(tipo_mensaje, 4)
        self.assertEqual(carga_util, b"abc")

    def test_decodificar_estado_lee_marcador_y_velocidad_con_signo(self):
        estado = decodificar_estado_juego(b"\x03\x04\x0a\x05\xff\x01\x02\x07")

        self.assertEqual(estado["paleta_izquierda_y"], 3)
        self.assertEqual(estado["paleta_derecha_y"], 4)
        self.assertEqual(estado["pelota_dx"], -1)
        self.assertEqual(estado["pelota_dy"], 1)
        self.assertEqual(estado["marcador_izquierdo"], 2)
        self.assertEqual(estado["marcador_derecho"], 7)


if __name__ == "__main__":
    unittest.main()
