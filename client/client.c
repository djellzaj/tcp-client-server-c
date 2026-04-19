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

    // Lidhja me serverin
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Nuk u lidh me serverin");
        return -1;
    }

    printf("U lidh me serverin!\n");
    printf("Shkruaj mesazhe (shkruaj 'exit' per me dal):\n");

    while (1) {
        printf("Ti: ");
        fgets(message, sizeof(message), stdin);

        // Dërgo mesazh
        send(sock, message, strlen(message), 0);

        // Nëse user shkruan "exit" → ndalo  
        if (strncmp(message, "exit", 4) == 0) {
            printf("Lidhja u mbyll.\n");
            break;
        }

        memset(buffer, 0, sizeof(buffer));


    return 0;
}