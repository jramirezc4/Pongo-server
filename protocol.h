#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Cada mensaje usa este formato:
 *
 *   [tipo: 1 byte][longitud: 2 bytes en orden de red][carga util]
 */
#define MENSAJE_REGISTRO 1
#define MENSAJE_MOVER_ARRIBA 2
#define MENSAJE_MOVER_ABAJO 3
#define MENSAJE_ESTADO_JUEGO 4
#define MENSAJE_MARCADOR 5
#define MENSAJE_ERROR 6
#define MENSAJE_SALUDO 7
#define MENSAJE_CONFIRMACION 8

#define ANCHO_TABLERO 20
#define ALTO_TABLERO 10
#define ALTO_PALETA 4
#define POSICION_INICIAL_PALETA 3
#define TAMANO_ESTADO_JUEGO 8

#endif
