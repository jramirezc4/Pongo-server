# Pong Server

Proyecto de redes para construir un juego Pong con arquitectura cliente/servidor.

## Estado actual

El servidor acepta una conexion TCP y el cliente puede enviar un mensaje binario
`HELLO` para recibir una respuesta `ACK`.

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
python3 client.py
```

El cliente se conecta por defecto a `127.0.0.1:9000`, muestra los mensajes de
conexion y desconexion, y termina despues de cerrar el socket. Tambien se puede
indicar otro servidor o puerto:

```bash
python3 client.py --host 127.0.0.1 --port 9000
```

## Protocolo binario inicial

Los mensajes usan un encabezado de 3 bytes:

- 1 byte para el tipo de mensaje.
- 2 bytes para la longitud de la carga útil, en orden de red.
- La carga útil, si existe.

En esta primera prueba, el cliente envía `HELLO` sin carga útil y el servidor
responde `ACK` sin carga útil.

## Autores

Proyecto desarrollado para la asignatura Internet: Arquitectura y Protocolos.
