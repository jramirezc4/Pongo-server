# Pong Server

Servidor multijugador de Pong implementado con sockets TCP. El servidor está
escrito en C y los clientes están escritos en Python. El proyecto incluye un
cliente de consola para probar el protocolo y una vista gráfica basada en
Tkinter.

## Características

- Comunicación cliente-servidor mediante TCP.
- Partidas de hasta dos jugadores.
- Registro de jugador mediante apodo y correo electrónico.
- Movimiento de paletas y actualización periódica de la pelota.
- Detección de rebotes, puntos y reinicio de la pelota.
- Difusión del estado de la partida a los clientes registrados.
- Registro de conexiones, movimientos, puntos y errores en la terminal y en un
  archivo de log.
- Cliente de consola para pruebas automatizadas o interacción con teclado.
- Cliente gráfico con controles de teclado.
- Compatibilidad del servidor con Windows y sistemas tipo Unix.

## Requisitos

### Servidor

- GCC o un compilador compatible con la interfaz de GCC.
- `make` para usar el `Makefile` (opcional).
- En Windows, MinGW-w64/MSYS2 y la biblioteca Winsock 2, que se enlaza
  automáticamente con `-lws2_32`.

### Cliente

- Python 3.9 o superior.
- En Windows, Tkinter debe estar incluido en la instalación de Python para
  utilizar la interfaz gráfica.

El proyecto no requiere paquetes externos de Python: utiliza únicamente la
biblioteca estándar.

## Estructura del proyecto

| Archivo | Descripción |
| --- | --- |
| `server.c` | Servidor TCP, lógica de la partida, clientes y logger. |
| `protocol.h` | Tipos de mensaje y constantes compartidas por el protocolo. |
| `client.py` | Cliente TCP de consola y funciones de codificación/decodificación. |
| `juego.py` | Cliente gráfico de Pong construido con Tkinter. |
| `tests/test_client.py` | Pruebas unitarias del protocolo del cliente Python. |
| `Makefile` | Comandos de compilación y limpieza del servidor. |
| `logs/server.log` | Archivo donde se guardan los eventos del servidor. |
| `guia-estudio-servidor-pong.pdf` | Material complementario de estudio del servidor. |
| `calendario-proyecto.txt` | Calendario local de desarrollo y entrega. |

## Compilación del servidor

Desde la raíz del proyecto, con `make` instalado:

```bash
make
```

Esto genera el ejecutable `server` en Linux/macOS o `server.exe` en Windows,
según el compilador utilizado.

También se puede compilar directamente:

### Linux o macOS

```bash
gcc -Wall -Wextra server.c -o server
```

### Windows con MinGW

```powershell
gcc -Wall -Wextra server.c -lws2_32 -o server.exe
```

Para limpiar el ejecutable en Linux/macOS:

```bash
make clean
```

En Windows, si el archivo generado es `server.exe`, se puede eliminar con:

```powershell
Remove-Item .\server.exe
```

## Ejecución del servidor

El servidor recibe exactamente dos argumentos:

```text
server <PUERTO> <ARCHIVO_LOG>
```

Ejemplos:

```bash
./server 9000 logs/server.log
```

```powershell
.\server.exe 9000 logs\server.log
```

El puerto debe ser un número entre `1` y `65535`. El servidor escucha en todas
las interfaces de red (`0.0.0.0`), por lo que puede recibir conexiones
locales o remotas si el firewall permite el puerto elegido.

Al iniciar correctamente se muestran mensajes similares a:

```text
Servidor Pong escuchando en el puerto 9000.
Esperando hasta 2 jugadores.
```

El archivo de log se abre en modo de anexado. Si la carpeta `logs` no existe,
debe crearse antes de iniciar el servidor:

```powershell
New-Item -ItemType Directory -Force .\logs
```

```bash
mkdir -p logs
```

## Uso del cliente de consola

El cliente se conecta por defecto a `127.0.0.1:9000`:

```bash
python client.py --apodo ana --correo ana@example.com
```

Si no se proporcionan `--apodo` y `--correo`, el programa los solicita
interactivamente:

```bash
python client.py
```

### Opciones

| Opción | Descripción |
| --- | --- |
| `--host HOST` | Dirección IP o nombre del servidor. Por defecto: `127.0.0.1`. |
| `--puerto PUERTO` | Puerto TCP. Por defecto: `9000`. |
| `--apodo APODO` | Nombre mostrado del jugador. |
| `--correo CORREO` | Correo del jugador. |
| `--mover arriba\|abajo` | Envía un movimiento al registrarse. |
| `--segundos N` | Tiempo que observa estados en modo no interactivo. Por defecto: `3`. |
| `--jugar` | Activa el modo interactivo de consola. |

Ejemplo de una prueba breve:

```bash
python client.py --host 127.0.0.1 --puerto 9000 \
  --apodo ana --correo ana@example.com --mover arriba --segundos 5
```

En Windows PowerShell:

```powershell
python .\client.py --host 127.0.0.1 --puerto 9000 `
  --apodo ana --correo ana@example.com --mover arriba --segundos 5
```

En modo interactivo:

```bash
python client.py --apodo ana --correo ana@example.com --jugar
```

Los controles del cliente de consola son:

- `W` o flecha arriba: mover la paleta hacia arriba.
- `S` o flecha abajo: mover la paleta hacia abajo.
- `Q`: salir.

Para probar dos jugadores, se deben abrir dos terminales y ejecutar un cliente
con un apodo diferente en cada una. El servidor asigna el primer cliente al
lado izquierdo y el segundo al lado derecho. Las conexiones adicionales son
rechazadas mientras los dos espacios estén ocupados.

## Uso del cliente gráfico

El cliente gráfico requiere una sesión con soporte para Tkinter:

```bash
python juego.py --apodo ana --correo ana@example.com
```

También acepta `--host` y `--puerto`:

```bash
python juego.py --host 192.168.1.10 --puerto 9000 \
  --apodo ana --correo ana@example.com
```

Controles:

- `W` o flecha arriba: mover la paleta.
- `S` o flecha abajo: mover la paleta.
- `Q` o `Esc`: cerrar la ventana y desconectarse.

La ventana muestra las dos paletas, la pelota y el marcador recibido desde el
servidor. La lógica de la partida permanece en el servidor; el cliente solo
envía movimientos y dibuja el estado.

## Protocolo de comunicación

Cada mensaje TCP tiene el siguiente formato:

```text
[tipo: 1 byte][longitud: 2 bytes en orden de red][carga útil]
```

La longitud es un entero sin signo de 16 bits en big-endian. Los mensajes
pueden llegar fragmentados en varias lecturas de TCP, por lo que ambos lados
acumulan bytes hasta completar el encabezado y la carga.

### Tipos de mensaje

| Valor | Nombre | Dirección | Carga |
| ---: | --- | --- | --- |
| `1` | `REGISTRO` | Cliente → servidor | `apodo\0correo\0` en UTF-8. |
| `2` | `MOVER_ARRIBA` | Cliente → servidor | Vacía. |
| `3` | `MOVER_ABAJO` | Cliente → servidor | Vacía. |
| `4` | `ESTADO_JUEGO` | Servidor → cliente | 8 bytes de estado. |
| `5` | `MARCADOR` | Reservado | No se utiliza actualmente. |
| `6` | `ERROR` | Servidor → cliente | Texto terminado en `\0`. |
| `7` | `SALUDO` | Cliente → servidor | Vacía. |
| `8` | `CONFIRMACION` | Servidor → cliente | Vacía. |

### Flujo de conexión

1. El cliente abre una conexión TCP.
2. Envía `SALUDO` sin carga útil.
3. El servidor responde `CONFIRMACION`.
4. El cliente envía `REGISTRO` con el apodo y el correo.
5. El servidor valida los datos y responde `CONFIRMACION` o `ERROR`.
6. El cliente puede enviar movimientos.
7. El servidor envía periódicamente `ESTADO_JUEGO` a todos los jugadores
   registrados.

El apodo no puede estar vacío. El correo debe contener texto después del
primer separador nulo y al menos un carácter `@`. El servidor admite como
máximo dos jugadores conectados al mismo tiempo.

### Estado de la partida

La carga de `ESTADO_JUEGO` contiene ocho bytes, en este orden:

| Byte | Campo | Rango o interpretación |
| ---: | --- | --- |
| `0` | `paleta_izquierda_y` | Posición vertical de la paleta izquierda. |
| `1` | `paleta_derecha_y` | Posición vertical de la paleta derecha. |
| `2` | `pelota_x` | Posición horizontal de la pelota. |
| `3` | `pelota_y` | Posición vertical de la pelota. |
| `4` | `pelota_dx` | Velocidad horizontal con signo. |
| `5` | `pelota_dy` | Velocidad vertical con signo. |
| `6` | `marcador_izquierdo` | Puntos del jugador izquierdo. |
| `7` | `marcador_derecho` | Puntos del jugador derecho. |

El tablero lógico mide `20 × 10` celdas, cada paleta tiene una altura de
`4` celdas y el servidor actualiza la pelota aproximadamente una vez por
segundo.

## Arquitectura y funcionamiento

El servidor utiliza un único proceso y `select()` para atender el socket de
escucha y los sockets de los dos jugadores sin bloquearse esperando a un
cliente concreto.

En cada ciclo:

1. Se revisan nuevas conexiones y mensajes pendientes.
2. Se procesan saludos, registros y movimientos.
3. Se actualiza la pelota cuando corresponde.
4. Se registra cualquier punto obtenido.
5. Se difunde el estado actualizado a los clientes registrados.

El servidor mantiene un único estado de partida. Cuando un jugador se
desconecta, su espacio queda disponible para una nueva conexión y el estado
existente no se reinicia automáticamente.

## Pruebas

Las pruebas unitarias cubren la construcción del encabezado binario, la lectura
de mensajes fragmentados y la decodificación del estado de juego:

```bash
python -m unittest discover -s tests -p "test_*.py" -v
```

También se puede ejecutar directamente:

```bash
python tests/test_client.py
```

Una prueba manual completa consiste en:

1. Compilar e iniciar el servidor.
2. Conectar un cliente y verificar el saludo y el registro.
3. Conectar un segundo cliente.
4. Mover ambas paletas y comprobar los estados mostrados.
5. Verificar el marcador y el archivo `logs/server.log`.
6. Cerrar los clientes y confirmar que el servidor registra las
   desconexiones.

## Ejecución en AWS EC2

En una instancia Linux con GCC:

```bash
sudo apt update
sudo apt install -y build-essential git python3
git clone <URL_DEL_REPOSITORIO>
cd pong-server
mkdir -p logs
make
./server 9000 logs/server.log
```

En el Security Group de la instancia se debe agregar una regla de entrada
TCP para el puerto `9000`, restringida a la IP del cliente cuando sea posible.
Después, desde otra máquina:

```bash
python client.py --host <IP_PUBLICA_EC2> --puerto 9000 \
  --apodo ana --correo ana@example.com
```

El servidor ya escucha en `0.0.0.0`; además del Security Group, se debe revisar
el firewall del sistema operativo si la conexión no funciona.

Para mantener el servidor ejecutándose después de cerrar la sesión SSH se
puede utilizar `tmux`, `screen` o un servicio de `systemd`. No se incluyen
credenciales ni configuraciones de AWS en este repositorio.

## Solución de problemas

### El servidor no inicia

- Compruebe que el puerto esté libre.
- Verifique que el puerto sea un número entre `1` y `65535`.
- Asegúrese de que la carpeta del archivo de log exista y tenga permisos de
  escritura.
- Revise el mensaje mostrado en la terminal y en el log.

### El cliente no puede conectarse

- Confirme que el servidor esté ejecutándose.
- Compruebe el host y el puerto usados por el cliente.
- En conexiones remotas, revise el Security Group y el firewall.
- Verifique que el servidor esté escuchando en la dirección esperada.

### El registro es rechazado

- El apodo no puede estar vacío.
- El correo debe contener un `@`.
- Solo se admiten dos conexiones simultáneas.

### La ventana gráfica no abre

- Ejecute el cliente desde un entorno con interfaz gráfica.
- Verifique que Python incluya Tkinter.
- En Linux, instale el paquete de Tkinter de la distribución si no está
  instalado, por ejemplo `python3-tk` en Debian/Ubuntu.

## Licencia

Este repositorio no declara actualmente una licencia explícita. Antes de
redistribuir el proyecto, debe definirse la licencia correspondiente.