#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#define CLOSE_SOCKET(descriptor_socket) closesocket(descriptor_socket)
#define SOCKET_ERROR_CODE WSAGetLastError()
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR_CODE errno
#define CLOSE_SOCKET(descriptor_socket) close(descriptor_socket)
#endif

#define TAMANO_ENCABEZADO 3
#define TAMANO_MAXIMO_MENSAJE 256
#define MAXIMO_JUGADORES 2
#define INTERVALO_JUEGO_MS 250

struct EstadoJuego {
    unsigned char paleta_izquierda_y;
    unsigned char paleta_derecha_y;
    unsigned char pelota_x;
    unsigned char pelota_y;
    signed char pelota_dx;
    signed char pelota_dy;
    unsigned char marcador_izquierdo;
    unsigned char marcador_derecho;
};

struct Cliente {
    SOCKET descriptor_socket;
    int conectado;
    int registrado;
    int lado;
    char apodo[64];
};

static FILE *archivo_registro;

static const char *nombre_tipo_mensaje(unsigned char tipo)
{
    switch (tipo) {
    case MENSAJE_REGISTRO:
        return "REGISTRO";
    case MENSAJE_MOVER_ARRIBA:
        return "MOVER_ARRIBA";
    case MENSAJE_MOVER_ABAJO:
        return "MOVER_ABAJO";
    case MENSAJE_ESTADO_JUEGO:
        return "ESTADO_JUEGO";
    case MENSAJE_MARCADOR:
        return "MARCADOR";
    case MENSAJE_ERROR:
        return "ERROR";
    case MENSAJE_SALUDO:
        return "SALUDO";
    case MENSAJE_CONFIRMACION:
        return "CONFIRMACION";
    default:
        return "DESCONOCIDO";
    }
}

static int iniciar_registro(const char *ruta_archivo)
{
    archivo_registro = fopen(ruta_archivo, "a");
    if (archivo_registro == NULL) {
        fprintf(stderr, "No se pudo abrir el archivo de registro: %s\n",
                ruta_archivo);
        return 0;
    }
    return 1;
}

static void cerrar_registro(void)
{
    if (archivo_registro != NULL) {
        fclose(archivo_registro);
        archivo_registro = NULL;
    }
}

static void registrar_evento(const char *formato, ...)
{
    char marca_tiempo[32];
    time_t hora_actual = time(NULL);
    struct tm *hora_local = localtime(&hora_actual);
    va_list argumentos;

    if (hora_local == NULL) {
        return;
    }

    strftime(marca_tiempo, sizeof(marca_tiempo), "%Y-%m-%d %H:%M:%S", hora_local);
    printf("[%s] ", marca_tiempo);
    va_start(argumentos, formato);
    vprintf(formato, argumentos);
    va_end(argumentos);
    printf("\n");

    if (archivo_registro != NULL) {
        fprintf(archivo_registro, "[%s] ", marca_tiempo);
        va_start(argumentos, formato);
        vfprintf(archivo_registro, formato, argumentos);
        va_end(argumentos);
        fprintf(archivo_registro, "\n");
        fflush(archivo_registro);
    }
}

static int recibir_todo(SOCKET descriptor_socket, char *bufer, int tamano)
{
    int recibidos = 0;

    while (recibidos < tamano) {
        int bytes = recv(descriptor_socket, bufer + recibidos, tamano - recibidos, 0);
        if (bytes <= 0) {
            return 0;
        }
        recibidos += bytes;
    }

    return 1;
}

static int enviar_todo(SOCKET descriptor_socket, const char *bufer, int tamano)
{
    int enviados = 0;

    while (enviados < tamano) {
        int bytes = send(descriptor_socket, bufer + enviados, tamano - enviados, 0);
        if (bytes <= 0) {
            return 0;
        }
        enviados += bytes;
    }

    return 1;
}

static int enviar_mensaje(SOCKET descriptor_socket, unsigned char tipo,
                        const unsigned char *carga_util, unsigned short tamano_carga)
{
    unsigned char encabezado[TAMANO_ENCABEZADO];

    encabezado[0] = tipo;
    encabezado[1] = (unsigned char)((tamano_carga >> 8) & 0xFF);
    encabezado[2] = (unsigned char)(tamano_carga & 0xFF);

    if (!enviar_todo(descriptor_socket, (const char *)encabezado, TAMANO_ENCABEZADO)) {
        return 0;
    }
    if (tamano_carga > 0 && carga_util != NULL) {
        if (!enviar_todo(descriptor_socket, (const char *)carga_util, tamano_carga)) {
            return 0;
        }
    }

    registrar_evento("Respuesta enviada: %s.", nombre_tipo_mensaje(tipo));
    return 1;
}

static void iniciar_estado(struct EstadoJuego *estado)
{
    estado->paleta_izquierda_y = POSICION_INICIAL_PALETA;
    estado->paleta_derecha_y = POSICION_INICIAL_PALETA;
    estado->pelota_x = ANCHO_TABLERO / 2;
    estado->pelota_y = ALTO_TABLERO / 2;
    estado->pelota_dx = 1;
    estado->pelota_dy = 1;
    estado->marcador_izquierdo = 0;
    estado->marcador_derecho = 0;
}

static void reiniciar_pelota(struct EstadoJuego *estado, signed char direccion)
{
    estado->pelota_x = ANCHO_TABLERO / 2;
    estado->pelota_y = ALTO_TABLERO / 2;
    estado->pelota_dx = direccion;
    estado->pelota_dy = 1;
}

static void construir_mensaje_estado(const struct EstadoJuego *estado,
                                     unsigned char *carga_util)
{
    carga_util[0] = estado->paleta_izquierda_y;
    carga_util[1] = estado->paleta_derecha_y;
    carga_util[2] = estado->pelota_x;
    carga_util[3] = estado->pelota_y;
    carga_util[4] = (unsigned char)estado->pelota_dx;
    carga_util[5] = (unsigned char)estado->pelota_dy;
    carga_util[6] = estado->marcador_izquierdo;
    carga_util[7] = estado->marcador_derecho;
}

static void enviar_error(SOCKET descriptor_socket, const char *mensaje)
{
    size_t longitud = strlen(mensaje);
    unsigned char carga_util[TAMANO_MAXIMO_MENSAJE];

    if (longitud >= sizeof(carga_util)) {
        longitud = sizeof(carga_util) - 1;
    }
    memcpy(carga_util, mensaje, longitud);
    carga_util[longitud] = '\0';
    enviar_mensaje(descriptor_socket, MENSAJE_ERROR, carga_util, (unsigned short)(longitud + 1));
}

static void difundir_estado(struct Cliente clientes[MAXIMO_JUGADORES],
                            const struct EstadoJuego *estado)
{
    unsigned char carga_util[TAMANO_ESTADO_JUEGO];
    int indice;

    construir_mensaje_estado(estado, carga_util);
    for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
        if (clientes[indice].conectado && clientes[indice].registrado) {
            if (!enviar_mensaje(clientes[indice].descriptor_socket, MENSAJE_ESTADO_JUEGO,
                              carga_util, sizeof(carga_util))) {
                clientes[indice].conectado = 0;
            }
        }
    }
}

static int buscar_cliente_libre(struct Cliente clientes[MAXIMO_JUGADORES])
{
    int indice;

    for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
        if (!clientes[indice].conectado) {
            return indice;
        }
    }
    return -1;
}

static int contar_jugadores_registrados(struct Cliente clientes[MAXIMO_JUGADORES])
{
    int indice;
    int cantidad = 0;

    for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
        if (clientes[indice].conectado && clientes[indice].registrado) {
            cantidad++;
        }
    }
    return cantidad;
}

static void desconectar_cliente(struct Cliente *Cliente)
{
    if (Cliente->conectado) {
        registrar_evento("Jugador desconectado: %s",
                  Cliente->apodo[0] ? Cliente->apodo : "sin registrar");
        CLOSE_SOCKET(Cliente->descriptor_socket);
    }
    memset(Cliente, 0, sizeof(*Cliente));
    Cliente->descriptor_socket = INVALID_SOCKET;
}

static int analizar_registro(struct Cliente *Cliente,
                              const unsigned char *carga_util,
                              unsigned short tamano_carga)
{
    char bufer[TAMANO_MAXIMO_MENSAJE];
    char *correo;
    size_t usados;

    if (carga_util == NULL || tamano_carga == 0 || tamano_carga >= sizeof(bufer)) {
        enviar_error(Cliente->descriptor_socket, "Datos de registro invalidos.");
        return 0;
    }

    usados = tamano_carga;
    memcpy(bufer, carga_util, usados);
    bufer[usados] = '\0';
    correo = strchr(bufer, '\0');

    if (bufer[0] == '\0') {
        enviar_error(Cliente->descriptor_socket, "El apodo no puede estar vacio.");
        return 0;
    }
    if (correo == NULL || correo[1] == '\0' || strchr(correo + 1, '@') == NULL) {
        enviar_error(Cliente->descriptor_socket, "El correo electronico es invalido.");
        return 0;
    }

    strncpy(Cliente->apodo, bufer, sizeof(Cliente->apodo) - 1);
    Cliente->apodo[sizeof(Cliente->apodo) - 1] = '\0';
    Cliente->registrado = 1;
    registrar_evento("Jugador %d registrado: %s", Cliente->lado + 1, Cliente->apodo);
    return enviar_mensaje(Cliente->descriptor_socket, MENSAJE_CONFIRMACION, NULL, 0);
}

static int mover_paleta(struct Cliente *Cliente, struct EstadoJuego *estado,
                       unsigned char tipo_mensaje)
{
    unsigned char *paleta;
    unsigned char posicion_anterior;

    paleta = Cliente->lado == 0
        ? &estado->paleta_izquierda_y
        : &estado->paleta_derecha_y;
    posicion_anterior = *paleta;

    if (tipo_mensaje == MENSAJE_MOVER_ARRIBA && *paleta > 0) {
        (*paleta)--;
    } else if (tipo_mensaje == MENSAJE_MOVER_ABAJO &&
               *paleta + ALTO_PALETA < ALTO_TABLERO) {
        (*paleta)++;
    }
    return *paleta != posicion_anterior;
}

static int actualizar_pelota(struct EstadoJuego *estado)
{
    int siguiente_x = estado->pelota_x + estado->pelota_dx;
    int siguiente_y = estado->pelota_y + estado->pelota_dy;

    if (siguiente_y <= 0 || siguiente_y >= ALTO_TABLERO - 1) {
        estado->pelota_dy = (signed char)-estado->pelota_dy;
        siguiente_y = estado->pelota_y + estado->pelota_dy;
    }

    if (siguiente_x <= 0) {
        if (siguiente_y >= estado->paleta_izquierda_y &&
            siguiente_y < estado->paleta_izquierda_y + ALTO_PALETA) {
            estado->pelota_dx = 1;
            siguiente_x = 1;
        } else {
            estado->marcador_derecho++;
            reiniciar_pelota(estado, 1);
            return 2;
        }
    } else if (siguiente_x >= ANCHO_TABLERO - 1) {
        if (siguiente_y >= estado->paleta_derecha_y &&
            siguiente_y < estado->paleta_derecha_y + ALTO_PALETA) {
            estado->pelota_dx = -1;
            siguiente_x = ANCHO_TABLERO - 2;
        } else {
            estado->marcador_izquierdo++;
            reiniciar_pelota(estado, -1);
            return 1;
        }
    }

    estado->pelota_x = (unsigned char)siguiente_x;
    estado->pelota_y = (unsigned char)siguiente_y;
    return 0;
}

static int procesar_mensaje_cliente(struct Cliente clientes[MAXIMO_JUGADORES],
                                 int indice_cliente,
                                 struct EstadoJuego *estado)
{
    struct Cliente *Cliente = &clientes[indice_cliente];
    unsigned char encabezado[TAMANO_ENCABEZADO];
    unsigned char *carga_util = NULL;
    unsigned short tamano_carga;
    int resultado = 1;

    if (!recibir_todo(Cliente->descriptor_socket, (char *)encabezado, TAMANO_ENCABEZADO)) {
        return 0;
    }

    tamano_carga = (unsigned short)((encabezado[1] << 8) | encabezado[2]);
    registrar_evento("Peticion recibida: %s (%u bytes).",
                     nombre_tipo_mensaje(encabezado[0]), tamano_carga);
    if (tamano_carga > 0) {
        carga_util = (unsigned char *)malloc(tamano_carga);
        if (carga_util == NULL ||
            !recibir_todo(Cliente->descriptor_socket, (char *)carga_util, tamano_carga)) {
            free(carga_util);
            return 0;
        }
    }

    if (encabezado[0] == MENSAJE_SALUDO && tamano_carga == 0) {
        registrar_evento("HELLO recibido del jugador %d.", Cliente->lado + 1);
        resultado = enviar_mensaje(Cliente->descriptor_socket, MENSAJE_CONFIRMACION, NULL, 0);
    } else if (encabezado[0] == MENSAJE_REGISTRO && !Cliente->registrado) {
        resultado = analizar_registro(Cliente, carga_util, tamano_carga);
        if (resultado) {
            difundir_estado(clientes, estado);
        }
    } else if (Cliente->registrado &&
               (encabezado[0] == MENSAJE_MOVER_ARRIBA || encabezado[0] == MENSAJE_MOVER_ABAJO) &&
               tamano_carga == 0) {
        int movio = mover_paleta(Cliente, estado, encabezado[0]);
        registrar_evento("Movimiento %s de %s: %s.",
                  encabezado[0] == MENSAJE_MOVER_ARRIBA ? "arriba" : "abajo",
                  Cliente->apodo,
                  movio ? "aceptado" : "limite alcanzado");
        difundir_estado(clientes, estado);
    } else {
        registrar_evento("Mensaje no soportado del jugador %d.", Cliente->lado + 1);
        enviar_error(Cliente->descriptor_socket, "Mensaje no soportado.");
    }

    free(carga_util);
    return resultado;
}

int main(int cantidad_argumentos, char *argumentos[])
{
    SOCKET socket_servidor;
    struct sockaddr_in direccion_servidor;
    struct Cliente clientes[MAXIMO_JUGADORES];
    struct EstadoJuego estado;
    int indice;
    int ultimo_tick;
    int ganador_punto;
    char *fin_puerto;
    long puerto;

#ifdef _WIN32
    WSADATA datos;
    WSAStartup(MAKEWORD(2, 2), &datos);
#endif

    if (cantidad_argumentos != 3) {
        fprintf(stderr, "Uso: %s <PUERTO> <ARCHIVO_LOG>\n", argumentos[0]);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    puerto = strtol(argumentos[1], &fin_puerto, 10);
    if (*argumentos[1] == '\0' || *fin_puerto != '\0' ||
        puerto < 1 || puerto > 65535) {
        fprintf(stderr, "El puerto debe ser un numero entre 1 y 65535.\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (!iniciar_registro(argumentos[2])) {
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    memset(clientes, 0, sizeof(clientes));
    for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
        clientes[indice].descriptor_socket = INVALID_SOCKET;
    }
    iniciar_estado(&estado);

    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor == INVALID_SOCKET) {
        registrar_evento("No se pudo crear el socket.");
        cerrar_registro();
        return 1;
    }

    memset(&direccion_servidor, 0, sizeof(direccion_servidor));
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = htonl(INADDR_ANY);
    direccion_servidor.sin_port = htons((unsigned short)puerto);

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor,
             sizeof(direccion_servidor)) < 0 ||
        listen(socket_servidor, MAXIMO_JUGADORES) < 0) {
        registrar_evento("No se pudo iniciar el servidor en el puerto %ld.", puerto);
        CLOSE_SOCKET(socket_servidor);
        cerrar_registro();
        return 1;
    }

    registrar_evento("Servidor Pong escuchando en el puerto %ld.", puerto);
    registrar_evento("Esperando hasta %d jugadores.", MAXIMO_JUGADORES);
    ultimo_tick = (int)time(NULL);

    while (1) {
        fd_set conjunto_lectura;
        struct timeval tiempo_espera;
        SOCKET socket_maximo = socket_servidor;
        int listos;

        FD_ZERO(&conjunto_lectura);
        FD_SET(socket_servidor, &conjunto_lectura);
        for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
            if (clientes[indice].conectado) {
                FD_SET(clientes[indice].descriptor_socket, &conjunto_lectura);
                if (clientes[indice].descriptor_socket > socket_maximo) {
                    socket_maximo = clientes[indice].descriptor_socket;
                }
            }
        }

        tiempo_espera.tv_sec = 0;
        tiempo_espera.tv_usec = INTERVALO_JUEGO_MS * 1000;
        listos = select((int)socket_maximo + 1, &conjunto_lectura, NULL, NULL, &tiempo_espera);
        if (listos < 0) {
            registrar_evento("Error al esperar conexiones: %d", SOCKET_ERROR_CODE);
            break;
        }

        if (FD_ISSET(socket_servidor, &conjunto_lectura)) {
            SOCKET socket_cliente = accept(socket_servidor, NULL, NULL);
            int indice_libre = buscar_cliente_libre(clientes);

            if (socket_cliente != INVALID_SOCKET && indice_libre >= 0) {
                clientes[indice_libre].descriptor_socket = socket_cliente;
                clientes[indice_libre].conectado = 1;
                clientes[indice_libre].lado = indice_libre;
                registrar_evento("Conexion recibida para jugador %d.", indice_libre + 1);
            } else if (socket_cliente != INVALID_SOCKET) {
                registrar_evento("Conexion rechazada: la partida esta llena.");
                enviar_error(socket_cliente, "La partida ya esta llena.");
                CLOSE_SOCKET(socket_cliente);
            }
        }

        for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
            if (clientes[indice].conectado &&
                FD_ISSET(clientes[indice].descriptor_socket, &conjunto_lectura) &&
                !procesar_mensaje_cliente(clientes, indice, &estado)) {
                desconectar_cliente(&clientes[indice]);
            }
        }

        if ((int)time(NULL) != ultimo_tick) {
            ultimo_tick = (int)time(NULL);
            if (contar_jugadores_registrados(clientes) > 0) {
                ganador_punto = actualizar_pelota(&estado);
                if (ganador_punto == 1) {
                    registrar_evento("Punto para el jugador izquierdo. Marcador %d-%d.",
                              estado.marcador_izquierdo, estado.marcador_derecho);
                } else if (ganador_punto == 2) {
                    registrar_evento("Punto para el jugador derecho. Marcador %d-%d.",
                              estado.marcador_izquierdo, estado.marcador_derecho);
                }
                difundir_estado(clientes, &estado);
            }
        }
    }

    for (indice = 0; indice < MAXIMO_JUGADORES; indice++) {
        desconectar_cliente(&clientes[indice]);
    }
    CLOSE_SOCKET(socket_servidor);
    cerrar_registro();

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
