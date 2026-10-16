#ifndef PROTOCOL_H
#define PROTOCOL_H

/*
 * Cada mensaje usa este formato:
 *
 *   [tipo: 1 byte][longitud: 2 bytes en orden de red][carga útil]
 */
#define MESSAGE_REGISTER 1
#define MESSAGE_MOVE_UP 2
#define MESSAGE_MOVE_DOWN 3
#define MESSAGE_GAME_STATE 4
#define MESSAGE_SCORE 5
#define MESSAGE_ERROR 6
#define MESSAGE_HELLO 7
#define MESSAGE_ACK 8

#define GAME_WIDTH 20
#define GAME_HEIGHT 10
#define PADDLE_HEIGHT 4
#define INITIAL_PADDLE_Y 3
#define GAME_STATE_SIZE 8

#endif
