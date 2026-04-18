#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define TIMEOUT 30

typedef struct {
    SOCKET fd;
    struct sockaddr_in addr;
    time_t last_active;
    int active;
} Client;

void initClients(Client clients[]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = INVALID_SOCKET;
        clients[i].active = 0;
        clients[i].last_active = 0;
    }
}

int addClient(Client clients[], SOCKET fd, struct sockaddr_in addr) {
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
    closesocket(clients[i].fd);
    clients[i].fd = INVALID_SOCKET;
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

void handle_list(SOCKET client_fd) {
    DIR *dir = opendir("server_storage");
    struct dirent *entry;
    char response[4096] = "";

    if (dir == NULL) {
        char *msg = "ERROR: cannot open folder\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            strcat(response, entry->d_name);
            strcat(response, "\n");
        }
    }

    closedir(dir);

    if (strlen(response) == 0) {
        strcpy(response, "No files found\n");
    }

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_read(SOCKET client_fd, char *filename) {
    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        char *msg = "ERROR: file not found\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    char response[4096];
    size_t bytes_read = fread(response, 1, sizeof(response) - 1, f);
    response[bytes_read] = '\0';

    fclose(f);

    if (bytes_read == 0) {
        strcpy(response, "File is empty\n");
    }

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_search(SOCKET client_fd, char *keyword) {
    DIR *dir = opendir("server_storage");
    struct dirent *entry;
    char response[4096] = "";

    if (dir == NULL) {
        char *msg = "ERROR: cannot open folder\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0 &&
            strstr(entry->d_name, keyword) != NULL) {
            strcat(response, entry->d_name);
            strcat(response, "\n");
        }
    }

    closedir(dir);

    if (strlen(response) == 0) {
        strcpy(response, "No matching files\n");
    }

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_info(SOCKET client_fd, char *filename) {
    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    struct stat file_stat;
    if (stat(path, &file_stat) != 0) {
        char *msg = "ERROR: file not found\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    char response[1024];
    char *time_str = ctime(&file_stat.st_mtime);

    snprintf(response, sizeof(response),
             "Filename: %s\nSize: %lld bytes\nLast modified: %s",
             filename,
             (long long)file_stat.st_size,
             time_str);

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_command(SOCKET client_fd, char *buffer) {
    if (strncmp(buffer, "/list", 5) == 0) {
        handle_list(client_fd);
    } else if (strncmp(buffer, "/read ", 6) == 0) {
        char *filename = buffer + 6;
        filename[strcspn(filename, "\r\n")] = 0;
        handle_read(client_fd, filename);
    } else if (strncmp(buffer, "/search ", 8) == 0) {
        char *keyword = buffer + 8;
        keyword[strcspn(keyword, "\r\n")] = 0;
        handle_search(client_fd, keyword);
    } 
    else if (strncmp(buffer, "/info ", 6) == 0) {
    char *filename = buffer + 6;
    filename[strcspn(filename, "\r\n")] = 0;
    handle_info(client_fd, filename);
}
else {
        char *msg = "Unknown command\n";
        send(client_fd, msg, (int)strlen(msg), 0);
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    SOCKET server_fd;
    struct sockaddr_in server_addr;

    Client clients[MAX_CLIENTS];
    initClients(clients);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Socket error\n");
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind error\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) == SOCKET_ERROR) {
        printf("Listen error\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("Server running on port %d\n", PORT);

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);

        SOCKET max_fd = server_fd;

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

        int activity = select((int)max_fd + 1, &fds, NULL, NULL, &tv);
        if (activity == SOCKET_ERROR) {
            printf("Select error\n");
            continue;
        }

        if (FD_ISSET(server_fd, &fds)) {
            struct sockaddr_in client_addr;
            int len = sizeof(client_addr);

            SOCKET client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
            if (client_fd == INVALID_SOCKET) {
                printf("Accept error\n");
                continue;
            }

            int idx = addClient(clients, client_fd, client_addr);

            if (idx == -1) {
                char *msg = "Server full\n";
                send(client_fd, msg, (int)strlen(msg), 0);
                closesocket(client_fd);
            } else {
                char *msg = "Connected\n";
                send(client_fd, msg, (int)strlen(msg), 0);
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

                    handle_command(clients[i].fd, buffer);
                }
            }
        }

        checkTimeout(clients);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}