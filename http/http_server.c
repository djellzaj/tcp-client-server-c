#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORTI 8080

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr;
    char buffer[1024];

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORTI);

    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_socket, 3);

    printf("Serveri HTTP eshte duke punuar ne portin %d...\n", PORTI);

    while (1) {
        client_socket = accept(server_socket, NULL, NULL);

        recv(client_socket, buffer, sizeof(buffer), 0);

        char response[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n\r\n"
            "Serveri po punon.";

        send(client_socket, response, strlen(response), 0);

        close(client_socket);
    }

    return 0;
}
