#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORTI 8080
#define BUFFER_SIZE 4096
#define STATS_FILE "shared/server_stats.txt"

// Funksion për të dërguar përgjigje HTTP
void dergo_pergjigje(int client_socket, const char *status, const char *content_type, const char *body) {
    char response[8192];

    snprintf(response, sizeof(response),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             status, content_type, strlen(body), body);

    send(client_socket, response, strlen(response), 0);
}

// GET /
void handle_root(int client_socket) {
    const char *body = "Serveri HTTP eshte aktiv.";
    dergo_pergjigje(client_socket, "200 OK", "text/plain", body);
}

// GET /stats duke lexuar nga file
void handle_stats(int client_socket) {
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

// 404
void handle_not_found(int client_socket) {
    const char *body = "404 Not Found";
    dergo_pergjigje(client_socket, "404 Not Found", "text/plain", body);
}

// Analizon kërkesën
void handle_request(int client_socket, const char *request) {
    if (strncmp(request, "GET /stats", 10) == 0) {
        handle_stats(client_socket);
    } else if (strncmp(request, "GET / ", 6) == 0 || strncmp(request, "GET /HTTP", 9) == 0) {
        handle_root(client_socket);
    } else {
        handle_not_found(client_socket);
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Krijimi i socket-it
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Gabim ne krijimin e socket-it");
        return 1;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Konfigurimi
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORTI);

    // Bind
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Gabim ne bind");
        close(server_socket);
        return 1;
    }

    // Listen
    if (listen(server_socket, 5) < 0) {
        perror("Gabim ne listen");
        close(server_socket);
        return 1;
    }

    printf("Serveri HTTP eshte duke punuar ne portin %d...\n", PORTI);
    printf("http://localhost:%d/\n", PORTI);
    printf("http://localhost:%d/stats\n", PORTI);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Gabim ne accept");
            continue;
        }

        // ✅ SHTIMI I RI: marrim IP-në e klientit
        char *client_ip = inet_ntoa(client_addr.sin_addr);
        printf("\nKlienti u lidh nga IP: %s\n", client_ip);

        memset(buffer, 0, sizeof(buffer));
        recv(client_socket, buffer, sizeof(buffer) - 1, 0);

        // ✅ SHTIMI I RI: logim më i mirë
        printf("\n--- Kerkese e re ---\n");
        printf("IP: %s\n", client_ip);
        printf("%s\n", buffer);

        handle_request(client_socket, buffer);

        close(client_socket);
    }

    close(server_socket);
    return 0;
}
