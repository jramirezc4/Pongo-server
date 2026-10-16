# Pong Server

Proyecto de redes para construir un juego Pong con arquitectura cliente/servidor.

## Estado actual

El servidor acepta hasta dos conexiones TCP, empareja los jugadores por orden
de llegada, valida el registro y mantiene un estado basico del juego. Recibe
movimientos, limita las paletas, mueve la pelota, detecta puntos y envia
actualizaciones periodicas a los clientes.

## Tecnologias

- Servidor: C
- Cliente: Python 3
- Comunicacion planeada: sockets Berkeley sobre TCP

## Archivos

- `server.c`: programa principal del servidor.
- `protocol.h`: constantes del protocolo de comunicacion.
- `client.py`: programa cliente.
- `Makefile`: instrucciones para compilar el servidor.
- `logs/`: carpeta para los archivos de registro.

## Compilacion

En un sistema con GCC se puede compilar con:

```bash
make
```

## Ejecucion actual

Primero se inicia el servidor:

```bash
./server
```

En otra terminal se ejecuta el cliente:

```bash
python3 client.py --nickname ana --email ana@correo.com
```

Para probar dos jugadores en la misma maquina, se ejecutan dos clientes en
terminales separadas:

```bash
python3 client.py --nickname ana --email ana@correo.com --move down
python3 client.py --nickname juan --email juan@correo.com --move up
```

El cliente se conecta por defecto a `127.0.0.1:9000`, muestra los mensajes de
conexion y desconexion, y termina despues de cerrar el socket. Tambien se puede
indicar otro servidor o puerto:

```bash
python3 client.py --host 127.0.0.1 --port 9000
```

## Protocolo binario actual

Los mensajes usan un encabezado de 3 bytes:

- 1 byte para el tipo de mensaje.
- 2 bytes para la longitud de la carga útil, en orden de red.
- La carga útil, si existe.

El cliente realiza estas etapas:

1. Envía `HELLO` sin carga útil.
2. Recibe `ACK` del servidor.
3. Solicita nickname y correo para registrarse.
4. Envía el registro como `REGISTER` con una carga útil en formato
   `nickname\0correo\0`.
5. El servidor responde con `ACK` o `ERROR`.
6. Si el registro es valido, el servidor envía el estado inicial y luego
   actualizaciones periódicas.

Los mensajes `MOVE_UP` y `MOVE_DOWN` no tienen carga útil. El servidor valida
que cada paleta permanezca dentro del tablero. El estado del juego ahora tiene
8 bytes: las seis posiciones/velocidades anteriores y los puntajes izquierdo y
derecho.

El estado del juego se codifica como 8 bytes sencillos:

- paleta izquierda
- paleta derecha
- posicion X de la pelota
- posicion Y de la pelota
- velocidad X de la pelota
- velocidad Y de la pelota
- puntaje izquierdo
- puntaje derecho

## Autores

Proyecto desarrollado para la asignatura Internet: Arquitectura y Protocolos.
