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
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len;

    Client clients[MAX_CLIENTS];
    initClients(clients);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
