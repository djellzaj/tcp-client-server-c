#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "192.168.1.9"
#define PORT 8080
#define BUFFER_SIZE 1024
#define LINE_SIZE 1024
#define DOWNLOAD_PREFIX "downloaded_"

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
int recv_line(SOCKET sock, char *line, int max_len) {
    int i = 0;
    char c;

    while (i < max_len - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) {
            return -1;
        }

        line[i++] = c;
        if (c == '\n') {
            break;
        }
    }

    line[i] = '\0';
    return i;
}

void trim_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
const char *get_basename(const char *path) {
    const char *slash1 = strrchr(path, '/');
    const char *slash2 = strrchr(path, '\\');
    const char *base = path;

    if (slash1 && slash2) {
        base = (slash1 > slash2) ? slash1 + 1 : slash2 + 1;
    } else if (slash1) {
        base = slash1 + 1;
    } else if (slash2) {
        base = slash2 + 1;
    }

    return base;
}

void receive_text_response(SOCKET sock) {
    char buffer[BUFFER_SIZE + 1];
    int total = 0;
    int timeout_ms = 700;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));
     while (1) {
        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) {
            break;
        }

        buffer[bytes] = '\0';
        printf("%s", buffer);
        total += bytes;

        if (bytes < BUFFER_SIZE) {
            break;
        }
    }

    if (total == 0) {
        printf("(Nuk u mor asnje pergjigje ose pergjigjja perfundoi.)\n");
    }
    timeout_ms = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));

    printf("\n");
}

int handle_upload(SOCKET sock, const char *local_path) {
    FILE *f = fopen(local_path, "rb");
    if (f == NULL) {
        printf("ERROR: nuk u hap file-i lokal: %s\n", local_path);
        return -1;
    }
     if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        printf("ERROR: nuk u lexua madhesia e file-it.\n");
        return -1;
    }

    long filesize = ftell(f);
    if (filesize < 0) {
        fclose(f);
        printf("ERROR: madhesi e pavlefshme.\n");
        return -1;
    }

    rewind(f);

    const char *filename = get_basename(local_path);

    char command[LINE_SIZE];
    snprintf(command, sizeof(command), "/upload %s %ld\n", filename, filesize);

    if (send_all(sock, command, (int)strlen(command)) == -1) {
        fclose(f);
        printf("ERROR: deshtoi dergimi i komandes upload.\n");
        return -1;
    }

    char line[LINE_SIZE];
    if (recv_line(sock, line, sizeof(line)) <= 0) {
        fclose(f);
        printf("ERROR: serveri nuk ktheu pergjigje per upload.\n");
        return -1;
    }
     if (strncmp(line, "READY", 5) != 0) {
        printf("%s", line);
        fclose(f);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, f)) > 0) {
        if (send_all(sock, buffer, (int)bytes_read) == -1) {
            fclose(f);
            printf("ERROR: deshtoi dergimi i permbajtjes se file-it.\n");
            return -1;
        }
    }

    fclose(f);
    memset(line, 0, sizeof(line));
    if (recv_line(sock, line, sizeof(line)) > 0) {
        printf("%s", line);
    } else {
        printf("Upload perfundoi, por nuk u mor konfirmimi final.\n");
    }

    return 0;
}
int handle_download(SOCKET sock, const char *filename) {
    char command[LINE_SIZE];
    snprintf(command, sizeof(command), "/download %s\n", filename);

    if (send_all(sock, command, (int)strlen(command)) == -1) {
        printf("ERROR: deshtoi dergimi i komandes download.\n");
        return -1;
    }

    char header[LINE_SIZE];
    if (recv_line(sock, header, sizeof(header)) <= 0) {
        printf("ERROR: serveri nuk ktheu pergjigje per download.\n");
        return -1;
    }

    if (strncmp(header, "FILE ", 5) != 0) {
        // Mund të jetë ERROR: file not found ose permission denied
        printf("%s", header);
        return -1;
    }

    char recv_filename[256];
    long filesize = 0;

    if (sscanf(header, "FILE %255s %ld", recv_filename, &filesize) != 2 || filesize < 0) {
        printf("ERROR: header i pavlefshem nga serveri.\n");
        return -1;
    }
char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s%s", DOWNLOAD_PREFIX, recv_filename);

    FILE *f = fopen(output_path, "wb");
    if (f == NULL) {
        printf("ERROR: nuk u krijua file-i lokal %s\n", output_path);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    long remaining = filesize;

    while (remaining > 0) {
        int chunk = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : (int)remaining;
        int bytes = recv(sock, buffer, chunk, 0);

        if (bytes <= 0) {
            fclose(f);
            printf("ERROR: download u nderpre.\n");
            return -1;
        }

        fwrite(buffer, 1, bytes, f);
        remaining -= bytes;
    }

    fclose(f);
    printf("Download successful -> %s (%ld bytes)\n", output_path, filesize);
    return 0;
}
void handle_simple_command(SOCKET sock, const char *command) {
    if (send_all(sock, command, (int)strlen(command)) == -1) {
        printf("ERROR: deshtoi dergimi i komandes.\n");
        return;
    }

    receive_text_response(sock);
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char input[LINE_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Gabim ne krijimin e socket-it.\n");
        WSACleanup();
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_por
    t = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        printf("IP adresa e pavlefshme.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Nuk u lidh me serverin.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }
    printf("U lidh me serverin %s:%d\n", SERVER_IP, PORT);

    receive_text_response(sock);

    printf("Komandat:\n");
    printf("  /list\n");
    printf("  /read <filename>\n");
    printf("  /search <keyword>\n");
    printf("  /info <filename>\n");
    printf("  /upload <local_path>\n");
    printf("  /download <filename>\n");
    printf("  /delete <filename>\n");
    printf("  exit\n\n");
      while (1) {
        printf(">> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        trim_newline(input);

        if (strcmp(input, "exit") == 0) {
            break;
        }

        if (strncmp(input, "/upload ", 8) == 0) {
            char *local_path = input + 8;
            while (*local_path == ' ') local_path++;

            if (*local_path == '\0') {
                printf("Perdorimi: /upload <local_path>\n");
                continue;
            }

            handle_upload(sock, local_path);
        }
        else if (strncmp(input, "/download ", 10) == 0) {
            char *filename = input + 10;
            while (*filename == ' ') filename++;

            if (*filename == '\0') {
                printf("Perdorimi: /download <filename>\n");
                continue;
            }

            handle_download(sock, filename);
        }
         else if (
            strncmp(input, "/list", 5) == 0 ||
            strncmp(input, "/read ", 6) == 0 ||
            strncmp(input, "/search ", 8) == 0 ||
            strncmp(input, "/info ", 6) == 0 ||
            strncmp(input, "/delete ", 8) == 0
        ) {
            char command[LINE_SIZE + 2];
            snprintf(command, sizeof(command), "%s\n", input);
            handle_simple_command(sock, command);
        }
        else {
            char message[LINE_SIZE + 2];
            snprintf(message, sizeof(message), "%s\n", input);
            handle_simple_command(sock, message);
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}