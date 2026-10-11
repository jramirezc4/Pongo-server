#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#define CLOSE_SOCKET(socket_fd) closesocket(socket_fd)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define CLOSE_SOCKET(socket_fd) close(socket_fd)
#endif

#define PORT 9000
#define HEADER_SIZE 3
#define MAX_MESSAGE_SIZE 256

struct GameState {
    unsigned char left_paddle_y;
    unsigned char right_paddle_y;
    unsigned char ball_x;
    unsigned char ball_y;
    signed char ball_dx;
    signed char ball_dy;
};

static int receive_all(SOCKET socket_fd, char *buffer, int size)
{
    int received = 0;

    while (received < size) {
        int bytes = recv(socket_fd, buffer + received, size - received, 0);
        if (bytes <= 0) {
            return 0;
        }
        received += bytes;
    }

    return 1;
}

static int send_all(SOCKET socket_fd, const char *buffer, int size)
{
    int sent = 0;

    while (sent < size) {
        int bytes = send(socket_fd, buffer + sent, size - sent, 0);
        if (bytes <= 0) {
            return 0;
        }
        sent += bytes;
    }

    return 1;
}

static int send_message(SOCKET socket_fd, unsigned char type,
                        const unsigned char *payload, unsigned short payload_size)
{
    unsigned char header[HEADER_SIZE];

    header[0] = type;
    header[1] = (unsigned char)((payload_size >> 8) & 0xFF);
    header[2] = (unsigned char)(payload_size & 0xFF);

    if (!send_all(socket_fd, (const char *)header, HEADER_SIZE)) {
        return 0;
    }

    if (payload_size > 0 && payload != NULL) {
        if (!send_all(socket_fd, (const char *)payload, payload_size)) {
            return 0;
        }
    }

    return 1;
}

static void send_error(SOCKET socket_fd, const char *message)
{
    size_t length = strlen(message);
    unsigned char payload[MAX_MESSAGE_SIZE];

    if (length >= sizeof(payload)) {
        length = sizeof(payload) - 1;
    }

    memcpy(payload, message, length);
    payload[length] = '\0';
    send_message(socket_fd, MESSAGE_ERROR, payload, (unsigned short)(length + 1));
}

static void initialize_state(struct GameState *state)
{
    state->left_paddle_y = INITIAL_PADDLE_Y;
    state->right_paddle_y = INITIAL_PADDLE_Y;
    state->ball_x = GAME_WIDTH / 2;
    state->ball_y = GAME_HEIGHT / 2;
    state->ball_dx = 1;
    state->ball_dy = 1;
}

static void build_game_state_message(const struct GameState *state,
                                     unsigned char *payload)
{
    payload[0] = state->left_paddle_y;
    payload[1] = state->right_paddle_y;
    payload[2] = state->ball_x;
    payload[3] = state->ball_y;
    payload[4] = (unsigned char)state->ball_dx;
    payload[5] = (unsigned char)state->ball_dy;
}

static int handle_registration(SOCKET socket_fd,
                              const unsigned char *payload,
                              unsigned short payload_size)
{
    char buffer[MAX_MESSAGE_SIZE];
    char *nickname;
    char *email;
    size_t used;
    int email_valid;

    if (payload_size == 0 || payload == NULL) {
        send_error(socket_fd, "Datos de registro vacios.");
        return 0;
    }

    used = payload_size;
    if (used >= sizeof(buffer)) {
        used = sizeof(buffer) - 1;
    }

    memcpy(buffer, payload, used);
    buffer[used] = '\0';

    nickname = buffer;
    email = strchr(nickname, '\0');
    if (email == NULL) {
        send_error(socket_fd, "El mensaje de registro tiene un formato invalido.");
        return 0;
    }
    email++;

    if (nickname[0] == '\0') {
        send_error(socket_fd, "El nickname no puede estar vacio.");
        return 0;
    }

    email_valid = (email[0] != '\0' && strchr(email, '@') != NULL);
    if (!email_valid) {
        send_error(socket_fd, "El correo electronico es invalido.");
        return 0;
    }

    printf("Nuevo jugador registrado: %s <%s>\n", nickname, email);

    if (!send_message(socket_fd, MESSAGE_ACK, NULL, 0)) {
        return 0;
    }

    {
        struct GameState state;
        unsigned char game_payload[6];

        initialize_state(&state);
        build_game_state_message(&state, game_payload);
        if (!send_message(socket_fd, MESSAGE_GAME_STATE, game_payload, sizeof(game_payload))) {
            return 0;
        }
    }

    return 1;
}

int main(void)
{
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("No se pudo crear el socket.\n");
        return 1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        printf("No se pudo usar el puerto.\n");
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    if (listen(server_socket, 1) < 0) {
        printf("No se pudo poner el servidor a escuchar.\n");
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("Servidor Pong escuchando en el puerto %d.\n", PORT);
    printf("Esperando un cliente...\n");

    SOCKET client_socket = accept(server_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        printf("No se pudo aceptar el cliente.\n");
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("Cliente conectado.\n");

    while (1) {
        unsigned char header[HEADER_SIZE];
        unsigned short payload_size;
        unsigned char *payload = NULL;

        if (!receive_all(client_socket, (char *)header, HEADER_SIZE)) {
            printf("Cliente desconectado.\n");
            break;
        }

        payload_size = (unsigned short)((header[1] << 8) | header[2]);

        if (payload_size > 0) {
            payload = (unsigned char *)malloc(payload_size);
            if (payload == NULL) {
                printf("No hay memoria suficiente para procesar el mensaje.\n");
                break;
            }
            if (!receive_all(client_socket, (char *)payload, payload_size)) {
                free(payload);
                printf("No se pudo recibir la carga del mensaje.\n");
                break;
            }
        }

        if (header[0] == MESSAGE_HELLO && payload_size == 0) {
            printf("Mensaje HELLO recibido. Enviando ACK.\n");
            if (!send_message(client_socket, MESSAGE_ACK, NULL, 0)) {
                free(payload);
                break;
            }
        } else if (header[0] == MESSAGE_REGISTER) {
            if (!handle_registration(client_socket, payload, payload_size)) {
                free(payload);
                break;
            }
        } else {
            printf("Mensaje no soportado: %u\n", header[0]);
            send_error(client_socket, "Mensaje no soportado.");
        }

        free(payload);
    }

    CLOSE_SOCKET(client_socket);
    CLOSE_SOCKET(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
