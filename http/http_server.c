#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORTI 8081
#define BUFFER_SIZE 4096
#define STATS_FILE "shared/server_stats.txt"

void dergo_pergjigje(SOCKET client_socket, const char *status, const char *content_type, const char *body) {
    char response[8192];

    sprintf(response,
            "HTTP/1.1 %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            status, content_type, (int)strlen(body), body);

    send(client_socket, response, (int)strlen(response), 0);
}

void handle_root(SOCKET client_socket) {
    const char *body =
        "{\n"
        "  \"message\": \"HTTP server is running\",\n"
        "  \"endpoint\": \"/stats\"\n"
        "}";
    dergo_pergjigje(client_socket, "200 OK", "application/json", body);
}

void handle_stats(SOCKET client_socket) {
    FILE *file = fopen(STATS_FILE, "r");

    if (file == NULL) {
        const char *json_body =
            "{\n"
            "  \"active_connections\": 0,\n"
            "  \"client_ips\": [],\n"
            "  \"message_count\": 0,\n"
            "  \"messages\": []\n"
            "}";
        dergo_pergjigje(client_socket, "200 OK", "application/json", json_body);
        return;
    }

    char body[8192];
    size_t total_read = fread(body, 1, sizeof(body) - 1, file);
    body[total_read] = '\0';
    fclose(file);

    dergo_pergjigje(client_socket, "200 OK", "application/json", body);
}

void handle_not_found(SOCKET client_socket) {
    const char *body = "404 Not Found";
    dergo_pergjigje(client_socket, "404 Not Found", "text/plain", body);
}

void handle_request(SOCKET client_socket, const char *request) {
    if (strncmp(request, "GET /stats", 10) == 0) {
        handle_stats(client_socket);
    } else if (strncmp(request, "GET / ", 6) == 0) {
        handle_root(client_socket);
    } else {
        handle_not_found(client_socket);
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("Socket error\n");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORTI);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind error on HTTP server\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    if (listen(server_socket, 5) == SOCKET_ERROR) {
        printf("Listen error on HTTP server\n");
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    printf("HTTP serveri duke punuar ne portin %d...\n", PORTI);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            printf("Accept error on HTTP server\n");
            continue;
        }

        char *client_ip = inet_ntoa(client_addr.sin_addr);
        printf("\nHTTP klienti nga IP: %s u lidh\n", client_ip);

        memset(buffer, 0, sizeof(buffer));
        int received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        if (received <= 0) {
            closesocket(client_socket);
            continue;
        }

        buffer[received] = '\0';

        printf("\n--- HTTP Kerkese ---\n%s\n", buffer);

        handle_request(client_socket, buffer);

        closesocket(client_socket);
    }

    closesocket(server_socket);
    WSACleanup();
    return 0;
}