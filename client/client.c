#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int sock;
    struct sockaddr_in server;
    char message[1024];
    char buffer[1024];

    //Krijimi i socket-it
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Gabim ne krijimin e socket");
        return -1;
    }

    //Konfigurimi
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("192.168.1.9");

   

    return 0;
}