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
// Merr saktësisht length bytes
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

// Heq \r dhe \n nga fundi
void trim_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
    // Kthen emrin bazë të file-it nga path-i
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

// Lexon përgjigje tekstuale derisa të skadojë timeout i shkurtër
void receive_text_response(SOCKET sock) {
    char buffer[BUFFER_SIZE + 1];
    int total = 0;

    // Timeout i shkurtër për të mos ngecur
    int timeout_ms = 700;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));