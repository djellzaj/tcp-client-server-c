#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define TIMEOUT 30

typedef struct {
    int fd;
    struct sockaddr_in addr;
    time_t last_active;
    int active;
} Client;
void initClients(Client clients[]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].active = 0;
        clients[i].last_active = 0;
    }
}
int addClient(Client clients[], int fd, struct sockaddr_in addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].fd = fd;
            clients[i].addr = addr;
            clients[i].last_active = time(NULL);
            clients[i].active = 1;
            return i;
        }
    }
    return -1;
}
void removeClient(Client clients[], int i) {
    close(clients[i].fd);
    clients[i].fd = -1;
    clients[i].active = 0;
}

void saveMessage(char *ip, int port, char *msg) {
    FILE *f = fopen("messages.txt", "a");
    if (f != NULL) {
        fprintf(f, "%s:%d -> %s\n", ip, port, msg);
        fclose(f);
    }
}

void checkTimeout(Client clients[]) {
    time_t now = time(NULL);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            if (now - clients[i].last_active > TIMEOUT) {
                char *ip = inet_ntoa(clients[i].addr.sin_addr);
                int port = ntohs(clients[i].addr.sin_port);
                printf("Timeout: %s:%d\n", ip, port);
                removeClient(clients, i);
            }
        }
    }
}
int main() {
    int server_fd;
    struct sockaddr_in server_addr;

    Client clients[MAX_CLIENTS];
    initClients(clients);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket error");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind error");
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen error");
        return 1;
    }

    printf("Server running on port %d\n", PORT);
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);

        int max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                FD_SET(clients[i].fd, &fds);
                if (clients[i].fd > max_fd) {
                    max_fd = clients[i].fd;
                }
            }
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int activity = select(max_fd + 1, &fds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(server_fd, &fds)) {
            struct sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);

            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
            if (client_fd < 0) {
                perror("Accept error");
                continue;
            }

            int idx = addClient(clients, client_fd, client_addr);

            if (idx == -1) {
                char *msg = "Server full\n";
                send(client_fd, msg, strlen(msg), 0);
                close(client_fd);
            } else {
                char *msg = "Connected\n";
                send(client_fd, msg, strlen(msg), 0);
            }
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active && FD_ISSET(clients[i].fd, &fds)) {
                char buffer[BUFFER_SIZE];
                int bytes = recv(clients[i].fd, buffer, BUFFER_SIZE - 1, 0);

                if (bytes <= 0) {
                    removeClient(clients, i);
                } else {
                    buffer[bytes] = '\0';
                    clients[i].last_active = time(NULL);

                    char *ip = inet_ntoa(clients[i].addr.sin_addr);
                    int port = ntohs(clients[i].addr.sin_port);

                    printf("%s:%d -> %s\n", ip, port, buffer);
                    saveMessage(ip, port, buffer);

                    char *reply = "OK\n";
                    send(clients[i].fd, reply, strlen(reply), 0);
                }
            }
        }

        checkTimeout(clients);
    }

    close(server_fd);
    return 0;
}