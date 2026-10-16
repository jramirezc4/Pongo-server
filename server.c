#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#define CLOSE_SOCKET(socket_fd) closesocket(socket_fd)
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
#define CLOSE_SOCKET(socket_fd) close(socket_fd)
#endif

#define PORT 9000
#define HEADER_SIZE 3
#define MAX_MESSAGE_SIZE 256
#define MAX_CLIENTS 2
#define GAME_TICK_MS 250

struct GameState {
    unsigned char left_paddle_y;
    unsigned char right_paddle_y;
    unsigned char ball_x;
    unsigned char ball_y;
    signed char ball_dx;
    signed char ball_dy;
    unsigned char left_score;
    unsigned char right_score;
};

struct Client {
    SOCKET socket_fd;
    int connected;
    int registered;
    int side;
    char nickname[64];
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
        return send_all(socket_fd, (const char *)payload, payload_size);
    }

    return 1;
}

static void initialize_state(struct GameState *state)
{
    state->left_paddle_y = INITIAL_PADDLE_Y;
    state->right_paddle_y = INITIAL_PADDLE_Y;
    state->ball_x = GAME_WIDTH / 2;
    state->ball_y = GAME_HEIGHT / 2;
    state->ball_dx = 1;
    state->ball_dy = 1;
    state->left_score = 0;
    state->right_score = 0;
}

static void reset_ball(struct GameState *state, signed char direction)
{
    state->ball_x = GAME_WIDTH / 2;
    state->ball_y = GAME_HEIGHT / 2;
    state->ball_dx = direction;
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
    payload[6] = state->left_score;
    payload[7] = state->right_score;
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

static void broadcast_state(struct Client clients[MAX_CLIENTS],
                            const struct GameState *state)
{
    unsigned char payload[GAME_STATE_SIZE];
    int index;

    build_game_state_message(state, payload);
    for (index = 0; index < MAX_CLIENTS; index++) {
        if (clients[index].connected && clients[index].registered) {
            if (!send_message(clients[index].socket_fd, MESSAGE_GAME_STATE,
                              payload, sizeof(payload))) {
                clients[index].connected = 0;
            }
        }
    }
}

static int find_free_client(struct Client clients[MAX_CLIENTS])
{
    int index;

    for (index = 0; index < MAX_CLIENTS; index++) {
        if (!clients[index].connected) {
            return index;
        }
    }
    return -1;
}

static int registered_count(struct Client clients[MAX_CLIENTS])
{
    int index;
    int count = 0;

    for (index = 0; index < MAX_CLIENTS; index++) {
        if (clients[index].connected && clients[index].registered) {
            count++;
        }
    }
    return count;
}

static void disconnect_client(struct Client *client)
{
    if (client->connected) {
        printf("Jugador desconectado: %s\n",
               client->nickname[0] ? client->nickname : "sin registrar");
        CLOSE_SOCKET(client->socket_fd);
    }
    memset(client, 0, sizeof(*client));
    client->socket_fd = INVALID_SOCKET;
}

static int parse_registration(struct Client *client,
                              const unsigned char *payload,
                              unsigned short payload_size)
{
    char buffer[MAX_MESSAGE_SIZE];
    char *email;
    size_t used;

    if (payload == NULL || payload_size == 0 || payload_size >= sizeof(buffer)) {
        send_error(client->socket_fd, "Datos de registro invalidos.");
        return 0;
    }

    used = payload_size;
    memcpy(buffer, payload, used);
    buffer[used] = '\0';
    email = strchr(buffer, '\0');

    if (buffer[0] == '\0') {
        send_error(client->socket_fd, "El nickname no puede estar vacio.");
        return 0;
    }
    if (email == NULL || email[1] == '\0' || strchr(email + 1, '@') == NULL) {
        send_error(client->socket_fd, "El correo electronico es invalido.");
        return 0;
    }

    strncpy(client->nickname, buffer, sizeof(client->nickname) - 1);
    client->nickname[sizeof(client->nickname) - 1] = '\0';
    client->registered = 1;
    printf("Jugador %d registrado: %s\n", client->side + 1, client->nickname);
    return send_message(client->socket_fd, MESSAGE_ACK, NULL, 0);
}

static void move_paddle(struct Client *client, struct GameState *state,
                        unsigned char message_type)
{
    unsigned char *paddle;

    paddle = client->side == 0
        ? &state->left_paddle_y
        : &state->right_paddle_y;

    if (message_type == MESSAGE_MOVE_UP && *paddle > 0) {
        (*paddle)--;
    } else if (message_type == MESSAGE_MOVE_DOWN &&
               *paddle + PADDLE_HEIGHT < GAME_HEIGHT) {
        (*paddle)++;
    }
}

static void update_ball(struct GameState *state)
{
    int next_x = state->ball_x + state->ball_dx;
    int next_y = state->ball_y + state->ball_dy;

    if (next_y <= 0 || next_y >= GAME_HEIGHT - 1) {
        state->ball_dy = (signed char)-state->ball_dy;
        next_y = state->ball_y + state->ball_dy;
    }

    if (next_x <= 0) {
        if (next_y >= state->left_paddle_y &&
            next_y < state->left_paddle_y + PADDLE_HEIGHT) {
            state->ball_dx = 1;
            next_x = 1;
        } else {
            state->right_score++;
            reset_ball(state, 1);
            return;
        }
    } else if (next_x >= GAME_WIDTH - 1) {
        if (next_y >= state->right_paddle_y &&
            next_y < state->right_paddle_y + PADDLE_HEIGHT) {
            state->ball_dx = -1;
            next_x = GAME_WIDTH - 2;
        } else {
            state->left_score++;
            reset_ball(state, -1);
            return;
        }
    }

    state->ball_x = (unsigned char)next_x;
    state->ball_y = (unsigned char)next_y;
}

static int handle_client_message(struct Client clients[MAX_CLIENTS],
                                 int client_index,
                                 struct GameState *state)
{
    struct Client *client = &clients[client_index];
    unsigned char header[HEADER_SIZE];
    unsigned char *payload = NULL;
    unsigned short payload_size;
    int result = 1;

    if (!receive_all(client->socket_fd, (char *)header, HEADER_SIZE)) {
        return 0;
    }

    payload_size = (unsigned short)((header[1] << 8) | header[2]);
    if (payload_size > 0) {
        payload = (unsigned char *)malloc(payload_size);
        if (payload == NULL ||
            !receive_all(client->socket_fd, (char *)payload, payload_size)) {
            free(payload);
            return 0;
        }
    }

    if (header[0] == MESSAGE_HELLO && payload_size == 0) {
        result = send_message(client->socket_fd, MESSAGE_ACK, NULL, 0);
    } else if (header[0] == MESSAGE_REGISTER && !client->registered) {
        result = parse_registration(client, payload, payload_size);
        if (result) {
            broadcast_state(clients, state);
        }
    } else if (client->registered &&
               (header[0] == MESSAGE_MOVE_UP || header[0] == MESSAGE_MOVE_DOWN) &&
               payload_size == 0) {
        move_paddle(client, state, header[0]);
        broadcast_state(clients, state);
    } else {
        send_error(client->socket_fd, "Mensaje no soportado.");
    }

    free(payload);
    return result;
}

int main(void)
{
    SOCKET server_socket;
    struct sockaddr_in server_address;
    struct Client clients[MAX_CLIENTS];
    struct GameState state;
    int index;
    int last_tick;

#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif

    memset(clients, 0, sizeof(clients));
    for (index = 0; index < MAX_CLIENTS; index++) {
        clients[index].socket_fd = INVALID_SOCKET;
    }
    initialize_state(&state);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("No se pudo crear el socket.\n");
        return 1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0 ||
        listen(server_socket, MAX_CLIENTS) < 0) {
        printf("No se pudo iniciar el servidor en el puerto %d.\n", PORT);
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("Servidor Pong escuchando en el puerto %d.\n", PORT);
    printf("Esperando hasta %d jugadores...\n", MAX_CLIENTS);
    last_tick = (int)time(NULL);

    while (1) {
        fd_set read_set;
        struct timeval timeout;
        SOCKET max_socket = server_socket;
        int ready;

        FD_ZERO(&read_set);
        FD_SET(server_socket, &read_set);
        for (index = 0; index < MAX_CLIENTS; index++) {
            if (clients[index].connected) {
                FD_SET(clients[index].socket_fd, &read_set);
                if (clients[index].socket_fd > max_socket) {
                    max_socket = clients[index].socket_fd;
                }
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = GAME_TICK_MS * 1000;
        ready = select((int)max_socket + 1, &read_set, NULL, NULL, &timeout);
        if (ready < 0) {
            printf("Error al esperar conexiones: %d\n", SOCKET_ERROR_CODE);
            break;
        }

        if (FD_ISSET(server_socket, &read_set)) {
            SOCKET client_socket = accept(server_socket, NULL, NULL);
            int free_index = find_free_client(clients);

            if (client_socket != INVALID_SOCKET && free_index >= 0) {
                clients[free_index].socket_fd = client_socket;
                clients[free_index].connected = 1;
                clients[free_index].side = free_index;
                printf("Conexion recibida para jugador %d.\n", free_index + 1);
            } else if (client_socket != INVALID_SOCKET) {
                send_error(client_socket, "La partida ya esta llena.");
                CLOSE_SOCKET(client_socket);
            }
        }

        for (index = 0; index < MAX_CLIENTS; index++) {
            if (clients[index].connected &&
                FD_ISSET(clients[index].socket_fd, &read_set) &&
                !handle_client_message(clients, index, &state)) {
                disconnect_client(&clients[index]);
            }
        }

        if ((int)time(NULL) != last_tick) {
            last_tick = (int)time(NULL);
            if (registered_count(clients) > 0) {
                update_ball(&state);
                broadcast_state(clients, &state);
            }
        }
    }

    for (index = 0; index < MAX_CLIENTS; index++) {
        disconnect_client(&clients[index]);
    }
    CLOSE_SOCKET(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
