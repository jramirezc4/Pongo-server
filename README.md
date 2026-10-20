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
- `juego.py`: vista grafica del juego con Tkinter.
- `Makefile`: instrucciones para compilar el servidor.
- `logs/`: carpeta para los archivos de registro.
- `tests/`: pruebas automaticas pequenas del protocolo del cliente.

## Compilacion

En un sistema con GCC se puede compilar con:

```bash
make
```

## Ejecucion actual

Primero se inicia el servidor:

```bash
./server 9000 logs/server.log
```

En otra terminal se ejecuta el cliente:

```bash
python3 client.py --apodo ana --correo ana@correo.com
```

El servidor recibe dos argumentos obligatorios: el puerto y el archivo de
registro. En Windows se puede iniciar así:

```powershell
.\server.exe 9000 logs\server.log
```

Para probar dos jugadores en la misma maquina, se ejecutan dos clientes en
terminales separadas:

```bash
python3 client.py --apodo ana --correo ana@correo.com --mover abajo
python3 client.py --apodo juan --correo juan@correo.com --mover arriba
```

Para jugar de forma interactiva en Windows:

```bash
python3 client.py --apodo ana --correo ana@correo.com --jugar
```

Durante la partida:

- `W` o flecha arriba: mover la paleta hacia arriba.
- `S` o flecha abajo: mover la paleta hacia abajo.
- `Q`: salir de la partida.

La vista grafica se ejecuta con:

```bash
python3 juego.py --apodo ana --correo ana@correo.com
```

Para iniciar el segundo jugador, abre otra terminal y ejecuta:

```bash
python3 juego.py --apodo juan --correo juan@correo.com
```

`client.py` se conserva como cliente de prueba por consola.

El cliente se conecta por defecto a `127.0.0.1:9000`, muestra los mensajes de
conexion y desconexion, y termina despues de cerrar el socket. Tambien se puede
indicar otro servidor o puerto:

```bash
python3 client.py --host 127.0.0.1 --puerto 9000
```

## Protocolo binario actual

Los mensajes usan un encabezado de 3 bytes:

- 1 byte para el tipo de mensaje.
- 2 bytes para la longitud de la carga útil, en orden de red.
- La carga útil, si existe.

El cliente realiza estas etapas:

1. Envía `HELLO` sin carga útil.
2. Recibe `ACK` del servidor.
3. Solicita apodo y correo para registrarse.
4. Envía el registro como `REGISTER` con una carga útil en formato
   `apodo\0correo\0`.
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

## Registro y pruebas

El servidor imprime los eventos importantes en la terminal y los guarda en
`logs/server.log`. Se registran conexiones, registros de jugadores,
movimientos, puntos, rechazos, desconexiones, peticiones recibidas y
respuestas enviadas.

Para ejecutar las pruebas automaticas del protocolo:

```bash
python3 -m unittest discover -s tests
```

Para una prueba manual completa:

1. Compilar e iniciar el servidor.
2. Conectar dos clientes con apodos y correos diferentes.
3. Usar `--mover arriba` y `--mover abajo` para generar movimientos.
4. Observar los estados y el marcador durante unos segundos.
5. Cerrar los clientes y revisar `logs/server.log`.

## Despliegue en AWS Academy

El servidor escucha en `0.0.0.0`, por lo que puede recibir conexiones
externas cuando el Security Group permita el puerto elegido. En una instancia
Ubuntu, se puede ejecutar:

```bash
sudo apt update
sudo apt install -y build-essential git python3-tk
git clone https://github.com/jramirezc4/Pongo-server.git
cd Pongo-server
make
mkdir -p logs
./server 9000 logs/server.log
```

En el Security Group de la instancia se debe agregar una regla TCP de entrada
para el puerto `9000`, idealmente limitada a la IP pública del cliente.

Desde otra computadora, se reemplaza `IP_PUBLICA_AWS` por la IPv4 pública de
la instancia:

```bash
python3 client.py --host IP_PUBLICA_AWS --puerto 9000 \
  --apodo ana --correo ana@correo.com
```

Para usar la ventana grafica desde Windows:

```powershell
python juego.py --host IP_PUBLICA_AWS --puerto 9000 `
  --apodo ana --correo ana@correo.com
```

El servidor debe mantenerse ejecutándose en la instancia. Para revisar el
logger desde otra conexión SSH:

```bash
tail -f logs/server.log
```

## Autores

Proyecto desarrollado para la asignatura Internet: Arquitectura y Protocolos.
