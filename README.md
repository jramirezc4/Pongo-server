# Pong Server

Proyecto de redes para construir un juego Pong con arquitectura cliente/servidor.

## Estado actual

Este es el primer avance del proyecto. Se creo la estructura inicial y se dejaron
definidos los tipos de mensajes que usara el protocolo.

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

Por ahora, el servidor solo muestra un mensaje inicial:

```bash
./server
```

El cliente se puede ejecutar con:

```bash
python3 client.py
```

La comunicacion de red y el juego se implementaran en los siguientes avances.

## Autores

Proyecto desarrollado para la asignatura Internet: Arquitectura y Protocolos.
