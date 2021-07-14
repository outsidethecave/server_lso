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


#define PORT 50000

#define TRUE 1
#define FALSE 0
#define BOOLSTRINGSIZE 2

#define MAX_CREDENTIALS_LENGTH 50

#define SIGNUP 1
#define LOGIN 2

typedef struct ClientThreadArg {
    int socket;
    int threadnum;     // di prova
} ClientThreadArg;

int listener_socket_fd = 0;
int new_socket = 0;
pthread_t clientThread_id[50];
int clientnum = 0;
int users_fd;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t signup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t login_lock = PTHREAD_MUTEX_INITIALIZER;


int userExists (char nickname[]) {

    char read_char[1];   // carattere letto
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
unsigned long hash(unsigned char *str) {

    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c;

    return hash;

}
int logIn (char credentials_buffer[]) {

    char read_char[1];   // carattere letto
    char nickname[MAX_CREDENTIALS_LENGTH] = {0};
    char password[MAX_CREDENTIALS_LENGTH] = {0};
    char nick_to_compare[MAX_CREDENTIALS_LENGTH] = {0};
    char password_to_compare[MAX_CREDENTIALS_LENGTH] = {0};

    int i = 1, j = 0, nick_flag = TRUE;
    for (; credentials_buffer[i] != '\n'; i++) {
        if (credentials_buffer[i] == '|') {
            nickname[i-1] = '\0';
            nick_flag = FALSE;
        }
        else {
            if (nick_flag) {
                nickname[i-1] = credentials_buffer[i];
            }
            else {
                password[j] = credentials_buffer[i];
                j++;
            }
        }
    }
    password[i] = '\0';

    printf("EXTRACTED NICKNAME AND PASSWORD: %s  %s\n", nickname, password);
    if (lseek(users_fd, 0, SEEK_SET) < 0) {
        perror("Errore di seek");
        exit(-1);
    }

    i = 0; j = 0; int nickname_found = FALSE; int password_reached = FALSE;
    while (read(users_fd, read_char, 1) > 0) {
        if (read_char[0] == ' ') {
            nick_to_compare[i] = '\0';
            printf("NICK TO COMPARE: %s\n", nick_to_compare);
            if (strcmp(nickname, nick_to_compare) == 0) {
                nickname_found = TRUE;
            }
            password_reached = TRUE;
            i++;
        }
        else if (read_char[0] == '\n') {
            password_to_compare[j] = '\0';
            printf("PASSWORD TO COMPARE: %s\n", password_to_compare);
            if (nickname_found == TRUE) {
                printf("NICKNAME FOUND, RELATIVE PASSWORD: %s\n", password_to_compare);
                if (strcmp(password, password_to_compare) == 0) {
                    printf("LOGIN SUCCESFUL");
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


void* clientThread (void* arg) {

    int thread_socket = ((ClientThreadArg*)arg)->socket;
    char client_buffer[50] = {0};
    int client_action;
    int signupResult, loginResult;

    while (1) {

        memset(client_buffer, 0, strlen(client_buffer));
        if (read(thread_socket, client_buffer, 50) <= 0) {
            printf("Errore di ricezione dal client %d: client disconnesso\n", thread_socket);
            close(thread_socket);
            pthread_exit(NULL);
        }

        client_action = client_buffer[0] - '0'; // Converte char a cifra singola nel rispettivo int


        switch (client_action) {

            case SIGNUP:
                pthread_mutex_lock(&signup_lock);
                signupResult = saveCredentialsToFile(client_buffer);
                pthread_mutex_unlock(&signup_lock);
                if (signupResult == TRUE) {
                    if (send(thread_socket, "1\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        printf("Errore di ricezione del client %d: client disconnesso\n", thread_socket);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        printf("Il client %d registra un utente\n\n", thread_socket);
                    }
                }
                else {
                    if (send(thread_socket, "0\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        printf("Errore di ricezione del client %d: client disconnesso\n", thread_socket);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
            break;

            case LOGIN:
                pthread_mutex_lock(&login_lock);
                printf("CREDENTIALS BUFFER: %s", client_buffer);
                loginResult = logIn(client_buffer);
                printf("LOGIN RESULT: %d\n", loginResult);
                pthread_mutex_unlock(&login_lock);
                if (loginResult == 0) {
                    if (send(thread_socket, "0\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        printf("Errore di ricezione del client %d: client disconnesso\n", thread_socket);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        printf("Il client %d effettua un login\n\n", thread_socket);
                    }
                }
                else if (loginResult == 1) {
                    if (send(thread_socket, "1\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        printf("Errore di ricezione del client %d: client disconnesso\n", thread_socket);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
                else {
                    if (send(thread_socket, "2\n", BOOLSTRINGSIZE, MSG_NOSIGNAL) < 0) {
                        printf("Errore di ricezione del client %d: client disconnesso\n", thread_socket);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
            break;

        }
        /*if (strcmp(buffer, "exit\n") == 0 || valread <= 0) {
            close(thread_socket);
            thread_socket = 0;
            memset(buffer, 0, 1024);
            printf("\nClient %d exited.\n\n", id);
            pthread_exit(NULL);
        }
        if (thread_socket != 0) {
            buffer[strcspn(buffer, "\n")] = '\0';
            printf("Client %d says: %s\n", id, buffer);
            valsend = send(thread_socket, server_msg, strlen(server_msg), 0);
            memset(buffer, 0, 1024);
        }*/

    }

}



int main(int argc, char const *argv[]) {

    struct sockaddr_in address;

    int opt = 1;
    int addrlen = sizeof(address);

    // Creazione socket del server
    if ((listener_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Creazione della socket fallita");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket created...\n");

    if (setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket options set...\n");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port
    if (bind(listener_socket_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket bound to port %d...\n", PORT);

    if (listen(listener_socket_fd, 50) < 0) {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }
    else
        printf("Listening...\n\n");

    users_fd = open("utenti.txt", O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if (users_fd == -1) {
      perror("Errore di apertura del file degli utenti");
      exit(-1);
    }

    while(1) {

        new_socket = accept(listener_socket_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Accept error");
            exit(EXIT_FAILURE);
        }
        else {
            printf("\nClient found.\n\n");
            ClientThreadArg* arg = (ClientThreadArg*)malloc(sizeof(ClientThreadArg));
            arg->socket = new_socket;
            arg->threadnum = clientnum+1;
            pthread_create(&(clientThread_id[clientnum]), NULL, &clientThread, arg);
        }

    }

    exit(0);
}
