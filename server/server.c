#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <time.h>
#include <dirent.h>
#include <direct.h>
#include <sys/stat.h>

#define SERVER_IP "0.0.0.0"
#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024
#define TIMEOUT 30
#define USER_DELAY_MS 200

int admin_assigned = 0;

typedef struct {
    SOCKET fd;
    struct sockaddr_in addr;
    time_t last_active;
    int active;
    int is_admin;
} Client;

void initClients(Client clients[]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = INVALID_SOCKET;
        clients[i].active = 0;
        clients[i].last_active = 0;
        clients[i].is_admin = 0;
    }
}

void ensure_server_storage() {
    struct stat st = {0};

    if (stat("server_storage", &st) == -1) {
        if (_mkdir("server_storage") == 0) {
            printf("server_storage folder created\n");
        } else {
            printf("ERROR: could not create server_storage folder\n");
        }
    }
}

int addClient(Client clients[], SOCKET fd, struct sockaddr_in addr) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].fd = fd;
            clients[i].addr = addr;
            clients[i].last_active = time(NULL);
            clients[i].active = 1;

            if (admin_assigned == 0) {
                clients[i].is_admin = 1;
                admin_assigned = 1;
            } else {
                clients[i].is_admin = 0;
            }

            return i;
        }
    }
    return -1;
}

void removeClient(Client clients[], int i) {
    if (clients[i].is_admin) {
        admin_assigned = 0;
    }

    closesocket(clients[i].fd);
    clients[i].fd = INVALID_SOCKET;
    clients[i].active = 0;
    clients[i].last_active = 0;
    clients[i].is_admin = 0;
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

int send_all(SOCKET sock, const char *data, int length) {
    int total_sent = 0;
    while (total_sent < length) {
        int sent = send(sock, data + total_sent, length - total_sent, 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

int recv_all(SOCKET sock, char *buffer, int length) {
    int total_received = 0;
    while (total_received < length) {
        int received = recv(sock, buffer + total_received, length - total_received, 0);
        if (received <= 0) {
            return -1;
        }
        total_received += received;
    }
    return total_received;
}

int is_read_only_command(const char *buffer) {
    return strncmp(buffer, "/list", 5) == 0 ||
           strncmp(buffer, "/read ", 6) == 0 ||
           strncmp(buffer, "/search ", 8) == 0 ||
           strncmp(buffer, "/info ", 6) == 0;
}

int is_admin_command(const char *buffer) {
    return strncmp(buffer, "/delete ", 8) == 0 ||
           strncmp(buffer, "/upload ", 8) == 0 ||
           strncmp(buffer, "/download ", 10) == 0;
}

int is_command(const char *buffer) {
    return buffer[0] == '/';
}

int is_valid_filename(const char *filename) {
    if (filename == NULL || filename[0] == '\0') {
        return 0;
    }

    if (strstr(filename, "..") != NULL) {
        return 0;
    }

    if (strchr(filename, '/') != NULL || strchr(filename, '\\') != NULL) {
        return 0;
    }

    if (strchr(filename, ':') != NULL) {
        return 0;
    }

    return 1;
}

void handle_list(SOCKET client_fd) {
    DIR *dir = opendir("server_storage");
    struct dirent *entry;
    char response[4096];
    int offset = 0;

    response[0] = '\0';

    if (dir == NULL) {
        char *msg = "ERROR: cannot open folder\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            int written = snprintf(response + offset, sizeof(response) - offset,
                                   "%s\n", entry->d_name);

            if (written < 0 || written >= (int)(sizeof(response) - offset)) {
                break;
            }

            offset += written;
        }
    }

    closedir(dir);

    if (offset == 0) {
        strcpy(response, "No files found\n");
    }

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_read(SOCKET client_fd, char *filename) {
    if (!is_valid_filename(filename)) {
        char *msg = "ERROR: invalid filename\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        char *msg = "ERROR: file not found\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int total_sent = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        if (send_all(client_fd, buffer, (int)bytes_read) == -1) {
            fclose(f);
            return;
        }
        total_sent += (int)bytes_read;
    }

    fclose(f);

    if (total_sent == 0) {
        char *msg = "File is empty\n";
        send(client_fd, msg, (int)strlen(msg), 0);
    }
}

void handle_search(SOCKET client_fd, char *keyword) {
    DIR *dir = opendir("server_storage");
    struct dirent *entry;
    char response[4096];
    int offset = 0;

    response[0] = '\0';

    if (dir == NULL) {
        char *msg = "ERROR: cannot open folder\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 &&
            strcmp(entry->d_name, "..") != 0 &&
            strstr(entry->d_name, keyword) != NULL) {

            int written = snprintf(response + offset, sizeof(response) - offset,
                                   "%s\n", entry->d_name);

            if (written < 0 || written >= (int)(sizeof(response) - offset)) {
                break;
            }

            offset += written;
        }
    }

    closedir(dir);

    if (offset == 0) {
        strcpy(response, "No matching files\n");
    }

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_info(SOCKET client_fd, char *filename) {
    if (!is_valid_filename(filename)) {
        char *msg = "ERROR: invalid filename\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    struct stat file_stat;
    if (stat(path, &file_stat) != 0) {
        char *msg = "ERROR: file not found\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        char *msg = "ERROR: could not open file info\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    FILETIME ftCreateUTC, ftAccessUTC, ftWriteUTC;
    if (!GetFileTime(hFile, &ftCreateUTC, &ftAccessUTC, &ftWriteUTC)) {
        CloseHandle(hFile);
        char *msg = "ERROR: could not get file time\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    CloseHandle(hFile);

    FILETIME ftCreateLocal, ftWriteLocal;
    SYSTEMTIME stCreateLocal, stWriteLocal;

    FileTimeToLocalFileTime(&ftCreateUTC, &ftCreateLocal);
    FileTimeToSystemTime(&ftCreateLocal, &stCreateLocal);

    FileTimeToLocalFileTime(&ftWriteUTC, &ftWriteLocal);
    FileTimeToSystemTime(&ftWriteLocal, &stWriteLocal);

    char response[1024];
    snprintf(
        response,
        sizeof(response),
        "Filename: %s\n"
        "Size: %lld bytes\n"
        "Created: %02d-%02d-%04d %02d:%02d:%02d\n"
        "Last modified: %02d-%02d-%04d %02d:%02d:%02d\n",
        filename,
        (long long)file_stat.st_size,
        stCreateLocal.wDay, stCreateLocal.wMonth, stCreateLocal.wYear,
        stCreateLocal.wHour, stCreateLocal.wMinute, stCreateLocal.wSecond,
        stWriteLocal.wDay, stWriteLocal.wMonth, stWriteLocal.wYear,
        stWriteLocal.wHour, stWriteLocal.wMinute, stWriteLocal.wSecond
    );

    send(client_fd, response, (int)strlen(response), 0);
}

void handle_delete(SOCKET client_fd, char *filename) {
    if (!is_valid_filename(filename)) {
        char *msg = "ERROR: invalid filename\n";
        send(client_fd, msg, (int)strlen(msg), 0);
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    if (remove(path) == 0) {
        char *msg = "File deleted successfully\n";
        send(client_fd, msg, (int)strlen(msg), 0);
    } else {
        char *msg = "ERROR: could not delete file\n";
        send(client_fd, msg, (int)strlen(msg), 0);
    }
}

void handle_upload(Client *client, char *filename, int filesize) {
    if (!client->is_admin) {
        char *msg = "ERROR: permission denied\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    if (!is_valid_filename(filename)) {
        char *msg = "ERROR: invalid filename\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    if (filesize <= 0) {
        char *msg = "ERROR: invalid file size\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        char *msg = "ERROR: cannot create file\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    char *ready = "READY\n";
    send(client->fd, ready, (int)strlen(ready), 0);

    char buffer[BUFFER_SIZE];
    int remaining = filesize;

    while (remaining > 0) {
        int chunk = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : remaining;
        int received = recv(client->fd, buffer, chunk, 0);

        if (received <= 0) {
            fclose(f);
            remove(path); // fshi file-in e papërfunduar
            char *msg = "ERROR: upload interrupted\n";
            send(client->fd, msg, (int)strlen(msg), 0);
            return;
        }

        fwrite(buffer, 1, received, f);
        remaining -= received;
        client->last_active = time(NULL);
    }

    fclose(f);

    char *msg = "Upload successful\n";
    send(client->fd, msg, (int)strlen(msg), 0);
}

void handle_download(Client *client, char *filename) {
    if (!client->is_admin) {
        char *msg = "ERROR: permission denied\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    if (!is_valid_filename(filename)) {
        char *msg = "ERROR: invalid filename\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "server_storage/%s", filename);

    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        char *msg = "ERROR: file not found\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (filesize < 0) {
        fclose(f);
        char *msg = "ERROR: could not read file size\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    char header[256];
    snprintf(header, sizeof(header), "FILE %s %ld\n", filename, filesize);

    if (send_all(client->fd, header, (int)strlen(header)) == -1) {
        fclose(f);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        if (send_all(client->fd, buffer, (int)bytes_read) == -1) {
            fclose(f);
            return;
        }
    }

    fclose(f);
}

void handle_text_message(Client *client, char *buffer) {
    char *msg = "Message received\n";
    send(client->fd, msg, (int)strlen(msg), 0);
}

void handle_command(Client *client, char *buffer) {
    if (is_admin_command(buffer) && !client->is_admin) {
        char *msg = "ERROR: permission denied\n";
        send(client->fd, msg, (int)strlen(msg), 0);
        return;
    }

    if (strncmp(buffer, "/list", 5) == 0) {
        handle_list(client->fd);

    } else if (strncmp(buffer, "/read ", 6) == 0) {
        char *filename = buffer + 6;
        filename[strcspn(filename, "\r\n")] = 0;
        handle_read(client->fd, filename);

    } else if (strncmp(buffer, "/search ", 8) == 0) {
        char *keyword = buffer + 8;
        keyword[strcspn(keyword, "\r\n")] = 0;
        handle_search(client->fd, keyword);

    } else if (strncmp(buffer, "/info ", 6) == 0) {
        char *filename = buffer + 6;
        filename[strcspn(filename, "\r\n")] = 0;
        handle_info(client->fd, filename);

    } else if (strncmp(buffer, "/delete ", 8) == 0) {
        char *filename = buffer + 8;
        filename[strcspn(filename, "\r\n")] = 0;
        handle_delete(client->fd, filename);

    } else if (strncmp(buffer, "/upload ", 8) == 0) {
        char filename[256];
        int filesize;

        if (sscanf(buffer + 8, "%255s %d", filename, &filesize) != 2) {
            char *msg = "ERROR: usage /upload <filename> <filesize>\n";
            send(client->fd, msg, (int)strlen(msg), 0);
            return;
        }

        handle_upload(client, filename, filesize);

    } else if (strncmp(buffer, "/download ", 10) == 0) {
        char *filename = buffer + 10;
        filename[strcspn(filename, "\r\n")] = 0;
        handle_download(client, filename);

    } else {
        char *msg = "Unknown command\n";
        send(client->fd, msg, (int)strlen(msg), 0);
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
    ensure_server_storage();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Socket error\n");
        WSACleanup();
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

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
    if (clients[idx].is_admin) {
        char *msg = "Connected as ADMIN\n";
        send(client_fd, msg, (int)strlen(msg), 0);
    } else {
        char *msg = "Connected as USER\n";
        send(client_fd, msg, (int)strlen(msg), 0);
    }
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

                    if (!clients[i].is_admin) {
                        Sleep(USER_DELAY_MS);
                    }

                    if (is_command(buffer)) {
                        handle_command(&clients[i], buffer);
                    } else {
                        handle_text_message(&clients[i], buffer);
                    }
                }
            }
        }

        checkTimeout(clients);
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}