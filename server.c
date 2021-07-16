#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "llist.h"

#define TRUE 1
#define FALSE 0
#define BOOLSTRINGSIZE 2
#define PORT 50000
#define MAX_CREDENTIALS_LENGTH 50
#define BUFFER_LENGTH 256
#define USER_FILE "utenti.txt"
#define SIGNUP 1
#define LOGIN 2


typedef struct ClientThreadArg {
    char nickname[MAX_CREDENTIALS_LENGTH];
    int socket;
} ClientThreadArg;


int users_fd;
List* activeUsersList = NULL;
pthread_t clientThread_id[50];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t signup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t login_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;


int userExists (char nickname[]) {

    char read_char[1];
    char nick_to_compare[MAX_CREDENTIALS_LENGTH];
    int password_reached = FALSE;

    if (lseek(users_fd, 0, SEEK_SET) < 0) {
        perror("Errore di seek");
        exit(-1);
    }

    int i = 0;
    while (read(users_fd, read_char, 1) > 0) {
        if (read_char[0] == ' ') {
            nick_to_compare[i] = '\0';
            if (strcmp(nickname, nick_to_compare) == 0) {
                return TRUE;
            }
            else {
                password_reached = TRUE;
            }
        }
        else if (read_char[0] == '\n') {
            memset(nick_to_compare, 0, MAX_CREDENTIALS_LENGTH);
            i = 0;
            password_reached = FALSE;
        }
        else if (password_reached == FALSE) {
            nick_to_compare[i] = read_char[0];
            i++;
        }
    }

    return FALSE;

}
int saveCredentialsToFile (char credentials_buffer[]) {

    char credentials[MAX_CREDENTIALS_LENGTH] = {0};
    char nickname[MAX_CREDENTIALS_LENGTH] = {0};

    int i = 1, nick_flag = TRUE;

    for (; credentials_buffer[i] != '\0'; i++) {
        if (credentials_buffer[i] == '|') {
            credentials[i-1] = ' ';
            nickname[i-1] = '\0';
            nick_flag = FALSE;
        }
        else {
            credentials[i-1] = credentials_buffer[i];
        }
        if (nick_flag) {
            nickname[i-1] = credentials_buffer[i];
        }
    }
    credentials[i] = '\0';

    if (userExists(nickname)) {
        return FALSE;
    }

    if (write(users_fd, credentials, strlen(credentials)) == -1) {
        perror("Errore di scrittura sul file degli utenti");
        exit(-1);
    }

    return TRUE;

}
int logIn (char credentials_buffer[MAX_CREDENTIALS_LENGTH], char nickname_to_fill[MAX_CREDENTIALS_LENGTH]) {

    char read_char[1];
    char nickname[MAX_CREDENTIALS_LENGTH];
    char password[MAX_CREDENTIALS_LENGTH];
    char nick_to_compare[MAX_CREDENTIALS_LENGTH];
    char password_to_compare[MAX_CREDENTIALS_LENGTH];

    int i = 1, j = 0, nick_flag = TRUE;
    for (; credentials_buffer[i] != '\n'; i++) {
        if (credentials_buffer[i] == '|') {
            nickname[i-1] = '\0';
            nickname_to_fill[i-1] = '\0';
            nick_flag = FALSE;
        }
        else {
            if (nick_flag) {
                nickname[i-1] = credentials_buffer[i];
                nickname_to_fill[i-1] = credentials_buffer[i];
            }
            else {
                password[j] = credentials_buffer[i];
                j++;
            }
        }
    }
    password[i] = '\0';

    if (lseek(users_fd, 0, SEEK_SET) < 0) {
        perror("Errore di seek");
        exit(-1);
    }

    i = 0; j = 0; int nickname_found = FALSE; int password_reached = FALSE;
    while (read(users_fd, read_char, 1) > 0) {
        if (read_char[0] == ' ') {
            nick_to_compare[i] = '\0';
            if (strcmp(nickname, nick_to_compare) == 0) {
                nickname_found = TRUE;
            }
            password_reached = TRUE;
            i++;
        }
        else if (read_char[0] == '\n') {
            password_to_compare[j] = '\0';
            if (nickname_found == TRUE) {
                if (strcmp(password, password_to_compare) == 0) {
                    return 0;
                }
                else
                    return 1;
            }
            memset(nick_to_compare, 0, MAX_CREDENTIALS_LENGTH);
            memset(password_to_compare, 0, MAX_CREDENTIALS_LENGTH);
            i = 0;
            j = 0;
            password_reached = FALSE;
            continue;
        }
        else {
            if (password_reached == FALSE) {
                nick_to_compare[i] = read_char[0];
                i++;
            }
            else {
                password_to_compare[j] = read_char[0];
                j++;
            }
        }
    }

    return 2;

}
void onClientDisconnection (char nickname[MAX_CREDENTIALS_LENGTH], int client_socket) {
    printf("Errore di ricezione dal client %d: client disconnesso\n", client_socket);
    activeUsersList = delete(activeUsersList, nickname, areEqual);
    memset(nickname, 0, MAX_CREDENTIALS_LENGTH);
}


void* clientThread (void* argument) {

    ClientThreadArg* arg = (ClientThreadArg*)argument;

    int thread_socket = arg->socket;

    char client_buffer[BUFFER_LENGTH];

    int client_action;
    int signupResult, loginResult;


    while (1) {

        memset(client_buffer, 0, strlen(client_buffer));
        if (read(thread_socket, client_buffer, 50) <= 0) {
            printf("Errore di ricezione dal client %d: client disconnesso\n", thread_socket);
            close(thread_socket);
            pthread_exit(NULL);
        }

        client_action = client_buffer[0] - '0';   // Converte char a cifra singola nel rispettivo int

        switch (client_action) {

            case SIGNUP:
                pthread_mutex_lock(&signup_lock);
                signupResult = saveCredentialsToFile(client_buffer);
                pthread_mutex_unlock(&signup_lock);
                if (signupResult == TRUE) {
                    if (send(thread_socket, "1\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        pthread_mutex_lock(&list_lock);
                        onClientDisconnection(arg->nickname, thread_socket);
                        pthread_mutex_unlock(&list_lock);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        printf("Il client %d registra un utente\n\n", thread_socket);
                    }
                }
                else {
                    if (send(thread_socket, "0\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        pthread_mutex_lock(&list_lock);
                        onClientDisconnection(arg->nickname, thread_socket);
                        pthread_mutex_unlock(&list_lock);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
            break;

            case LOGIN:
                pthread_mutex_lock(&login_lock);
                loginResult = logIn(client_buffer, arg->nickname);
                pthread_mutex_unlock(&login_lock);
                if (loginResult == 0) {
                    if (send(thread_socket, "0\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        pthread_mutex_lock(&list_lock);
                        onClientDisconnection(arg->nickname, thread_socket);
                        pthread_mutex_unlock(&list_lock);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        pthread_mutex_lock(&list_lock);
                        activeUsersList = append(activeUsersList, arg->nickname);
                        pthread_mutex_unlock(&list_lock);
                        printf("%s si connette sul client %d\n\n", arg->nickname, thread_socket);
                    }
                }
                else if (loginResult == 1) {
                    memset(arg->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(thread_socket, "1\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        pthread_mutex_lock(&list_lock);
                        onClientDisconnection(arg->nickname, thread_socket);
                        pthread_mutex_unlock(&list_lock);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
                else {
                    memset(arg->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(thread_socket, "2\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        pthread_mutex_lock(&list_lock);
                        onClientDisconnection(arg->nickname, thread_socket);
                        pthread_mutex_unlock(&list_lock);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
            break;

        }
        // buffer[strcspn(buffer, "\n")] = '\0';

    }

}



int main(int argc, char const *argv[]) {

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int options = 1;

    int listener_socket_fd = 0, new_socket = 0;
    int clientnum = 0;

    ClientThreadArg* arg;



    users_fd = open(USER_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if (users_fd == -1) {
      perror("Errore di apertura del file degli utenti");
      exit(-1);
    }



    if ((listener_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Creazione della socket fallita");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket di ascolto creata...\n");

    if (setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &options, sizeof(options))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    else
        printf("Opzioni socket impostate...\n");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(listener_socket_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    else
        printf("Bound alla porta %d effettuato...\n", PORT);

    if (listen(listener_socket_fd, 50) < 0) {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }
    else
        printf("In ascolto...\n\n");



    while(1) {

        new_socket = accept(listener_socket_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept error");
            exit(EXIT_FAILURE);
        }
        else {
            printf("\nClient found.\n\n");
            arg = (ClientThreadArg*)malloc(sizeof(ClientThreadArg));
            arg->socket = new_socket;
            pthread_create(&(clientThread_id[clientnum]), NULL, &clientThread, arg);
        }

    }

    exit(0);
}
