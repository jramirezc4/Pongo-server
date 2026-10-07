#include <stdio.h>
#include "protocol.h"

#include <stdlib.h>
#include <string.h>

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

    unsigned char header[HEADER_SIZE];
    if (!receive_all(client_socket, (char *)header, HEADER_SIZE)) {
        printf("No se pudo recibir el mensaje.\n");
        CLOSE_SOCKET(client_socket);
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    unsigned short payload_size =
        (unsigned short)((header[1] << 8) | header[2]);
    if (header[0] != MESSAGE_HELLO || payload_size != 0) {
        printf("Mensaje no reconocido.\n");
        CLOSE_SOCKET(client_socket);
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    unsigned char response[HEADER_SIZE] = {MESSAGE_ACK, 0, 0};
    if (!send_all(client_socket, (const char *)response, HEADER_SIZE)) {
        printf("No se pudo enviar la respuesta.\n");
        CLOSE_SOCKET(client_socket);
        CLOSE_SOCKET(server_socket);
        return 1;
    }

    printf("Mensaje HELLO recibido. Respuesta ACK enviada.\n");
    CLOSE_SOCKET(client_socket);
    CLOSE_SOCKET(server_socket);

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
