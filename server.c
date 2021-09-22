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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include "llist.h"
#include "definizioni.h"
#include "funzioni.h"


int NUMBER_OF_PLAYERS;
int GRIDSIZE;
int WIN;

const char* Weekdays[] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};

int users_fd;
int user_events_fd;
List* clients                         = NULL;
List* activeUsersList                 = NULL;
List* clientsLookingForMatch          = NULL;
List* games                           = NULL;
ulong gamenum                         = 0;

pthread_mutex_t users_file_lock       = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_events_file_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clients_lock          = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t activeUsers_lock      = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lookingForMatch_lock  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t games_lock            = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gamenum_lock          = PTHREAD_MUTEX_INITIALIZER;

sigset_t set;



int main (int argc, char* argv[]) {

    if (argc != 2) {
        printf("\nArgomenti non validi: bisogna passare come parametro il numero di giocatori per partita, compreso tra 2 e 10\n\n");
        exit(EXIT_FAILURE);
    }

    if (atoi(argv[1]) < 2 || atoi(argv[1]) > 10) {
        printf("\nIl numero di giocatori per partita deve essere compreso tra 2 e 10\n\n");
        exit(EXIT_FAILURE);
    }

    // La prima operazione in assoluto è l'impostazione del numero di giocatori,
    // da cui si impostano la dimensione della griglia e il numero di territori da conquistare
    NUMBER_OF_PLAYERS = atoi(argv[1]);
    setGridSizeAndWinCondition();

    prepareRand();

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int options = 1;

    int listener_socket_fd    = 0;
    int new_socket            = 0;
    ulong clientnum           = 0;

    Client* client;

    // Blocco di SIGPIPE
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        perror("pthread_sigmask");
        exit(-1);
    }

    // Apertura del file degli utenti
    users_fd = open(USERS_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if (users_fd == -1) {
      perror("Errore di apertura del file degli utenti");
      exit(-1);
    }



    // Apertura del file di log
    user_events_fd = open(USER_EVENTS_FILE, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
    if (user_events_fd == -1) {
      perror("Errore di apertura del file degli eventi degli utenti");
      exit(-1);
    }



    // Creazione della socket
    if ((listener_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Creazione della socket fallita");
        exit(EXIT_FAILURE);
    }
    printf("Socket di ascolto creata...\n");

    // Impostazione delle opzioni di riutilizzo dell'indirizzo e della porta (prevenzione)
    if (setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &options, sizeof(options))) {
        perror("Errore durante l'impostazione delle opzioni della socket");
        exit(EXIT_FAILURE);
    }
    printf("Opzioni socket impostate...\n");

    address.sin_family      = AF_INET;                  // Impostazione della famiglia di indirizzi a IPv4
    address.sin_addr.s_addr = htonl(INADDR_ANY);        // Impostazione degli indirizzi ammissibili (tutti)
    address.sin_port        = htons(PORT);              // Impostazione della porta

    // Bind della socket all'indirizzo e alla porta
    if (bind(listener_socket_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind fallito");
        exit(EXIT_FAILURE);
    }
    printf("Bound alla porta %d effettuato...\n", PORT);

    // Messa in ascolto della socket
    if (listen(listener_socket_fd, BACKLOG) < 0) {
        perror("Listen fallito");
        exit(EXIT_FAILURE);
    }
    printf("In ascolto...\n\n");



    while(1) {

        // Accetta un nuovo client
        new_socket = accept(listener_socket_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Errore di accept");
            exit(EXIT_FAILURE);
        }

        printf("Client %d trovato.\n\n", new_socket);

        client = createClient(clientnum, new_socket);

        log_ClientConnection(new_socket, address.sin_addr);

        pthread_mutex_lock(&clients_lock);
        clients = append(clients, client);
        pthread_mutex_unlock(&clients_lock);

        pthread_create(&client->thread, NULL, clientThread, client);

        clientnum++;

    }

    exit(0);

}
