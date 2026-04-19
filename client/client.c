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