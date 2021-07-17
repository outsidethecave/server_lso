// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define PRIVATE_IP "192.168.1.77"
#define PUBLIC_IP "93.44.75.45"
#define PORT 50000

int main(int argc, char const *argv[]) {

    int client_socket = 0;
    int valread;

    struct sockaddr_in serv_addr;

    char message_to_send[1024];
    char buffer[1024] = {0};

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, PRIVATE_IP, &serv_addr.sin_addr) <= 0) {
        perror("Address error");
        return -1;
    }

    if (connect(client_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed ");
        return -1;
    }

    while (1) {

        printf("Send: ");
        fgets(message_to_send, 1024, stdin);
        fflush(stdin);
        send(client_socket, message_to_send, strlen(message_to_send), MSG_NOSIGNAL);
        if (strcmp(message_to_send, "exit\n") == 0) {
            memset(buffer, 0, 1024);
            close(client_socket);
            exit(EXIT_SUCCESS);
        }
        valread = read(client_socket, buffer, 1024);
        printf("Server: %s\n\n", buffer);
        memset(buffer, 0, 1024);

    }

    exit(0);
}
