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
