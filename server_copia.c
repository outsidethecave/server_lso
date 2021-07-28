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
#include <time.h>
#include "llist.h"

#define TRUE 1
#define FALSE 0

#define PORT 50000

#define MAX_CREDENTIALS_LENGTH 50
#define BUFFER_LENGTH 256
#define SYMBOL_SIZE 3

#define USER_FILE "utenti.txt"

#define SIGNUP 1
#define SIGNUP_SUCCESS "0\n"
#define USER_ALREADY_EXISTS "1\n"

#define LOGIN 2
#define LOGIN_SUCCESS "0\n"
#define USER_DOESNT_EXIST "1\n"
#define WRONG_PASSWORD "2\n"

#define VIEW_ACTIVE_USERS 3
#define LOOK_FOR_MATCH 4

#define STOP_LOOKING_FOR_MATCH 5
#define START_MATCH "0\n"
#define LEAVE_QUEUE "1\n"

#define GAME_ACTION 6

#define LEAVE_MATCH 7
#define MATCH_LEFT "0\n"

#define LOGOUT 8

#define NUMBER_OF_PLAYERS 3
#define GRIDSIZE 7
#define WIN 10

typedef char Symbol[SYMBOL_SIZE];

typedef struct Client {
    int clientSerial;
    char nickname[MAX_CREDENTIALS_LENGTH];
    int socket;
    int pipe[2];
    int activeGame;
    int positionInArrayOfPlayers;
    pthread_t thread;
} Client;

typedef struct Player {
    Client* client;
    Symbol symbol;
    int territories;
    int x;
    int y;
    int isActive;
} Player;

typedef struct Game {
    int gameSerial;
    Symbol** grid;
    Player* players[NUMBER_OF_PLAYERS];

    Player* activePlayer;
} Game;

int areEqual_cli (void* p1, void* p2) {
    Client* arg1 = (Client*)p1;
    Client* arg2 = (Client*)p2;

    return strcmp(arg1->nickname, arg2->nickname) == 0;
}

void printNode_cli (List* node) {
    Client* data = (Client*)(node->data);
    if (node->next) {
        printf("(%s, %d, %lu) -> ", data->nickname, data->socket, data->thread);
    }
    else {
        printf("(%s, %d, %lu) -> NULL\n", data->nickname, data->socket, data->thread);
    }
}


int users_fd;
int gamenum = 0;
List* activeUsersList = NULL;
List* clientsLookingForMatch = NULL;
Game* gameArgs[40];
pthread_t gameThreads[40];
pthread_mutex_t playerLocks[40];
pthread_mutex_t gamenum_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t signup_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t login_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t activeUsers_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lookingForMatch_lock = PTHREAD_MUTEX_INITIALIZER;


int userExists (char nickname[]) {

    char read_char[1];
    char nick_to_compare[MAX_CREDENTIALS_LENGTH];
    int password_reached = FALSE;

    if (lseek(users_fd, 0, SEEK_SET) < 0) {
        perror("Errore di seek");
        exit(EXIT_FAILURE);
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

    int i, nick_flag = TRUE;

    for (i = 1; credentials_buffer[i] != '\0'; i++) {
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
        exit(EXIT_FAILURE);
    }

    return TRUE;

}
int logIn (char credentials_buffer[MAX_CREDENTIALS_LENGTH], Client* clientArg) {

    char read_char[1];
    char nickname[MAX_CREDENTIALS_LENGTH];
    char password[MAX_CREDENTIALS_LENGTH];
    char nick_to_compare[MAX_CREDENTIALS_LENGTH];
    char password_to_compare[MAX_CREDENTIALS_LENGTH];

    int i = 1, j = 0, nick_flag = TRUE;
    for (; credentials_buffer[i] != '\n'; i++) {
        if (credentials_buffer[i] == '|') {
            nickname[i-1] = '\0';
            clientArg->nickname[i-1] = '\0';
            nick_flag = FALSE;
        }
        else {
            if (nick_flag) {
                nickname[i-1] = credentials_buffer[i];
                clientArg->nickname[i-1] = credentials_buffer[i];
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
        exit(EXIT_FAILURE);
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
                    return 0;   // LOGIN SUCCESS
                }
                else
                    return 2;   // WRONG PASSWORD
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

    return 1;    // USER NOT FOUND (DOESN'T EXIST)

}
void onClientDisconnection (Client* clientArg) {

    // QUI BISOGNA FARE ANCHE LE DOVUTE OPERAZIONI DI ELIMINAZIONE DALLA PARTITA

    printf("Errore di ricezione dal client %d: client disconnesso\n", clientArg->socket);

    pthread_mutex_lock(&activeUsers_lock);
    activeUsersList = delete(activeUsersList, clientArg->nickname, areEqual_str, NULL);
    pthread_mutex_unlock(&activeUsers_lock);
    pthread_mutex_lock(&lookingForMatch_lock);
    clientsLookingForMatch = delete(clientsLookingForMatch, clientArg, areEqual_str, NULL);
    pthread_mutex_unlock(&lookingForMatch_lock);

    memset(clientArg->nickname, 0, MAX_CREDENTIALS_LENGTH);

    free(clientArg);

}
int sendUsersList (Client* clientArg) {

    List* p = activeUsersList;
    int nick_size;
    char* nickname;

    while (p) {
        nick_size = strlen((char*)(p->data)) + 1;
        nickname = (char*)malloc(nick_size * sizeof(char));
        strcpy(nickname, (char*)(p->data));
        strcat(nickname, "\n");
        if (send(clientArg->socket, nickname, nick_size, MSG_NOSIGNAL) < 0) {
            return FALSE;
        }
        free(nickname);
        p = p->next;
    }
    if (send(clientArg->socket, "|\n", 2, MSG_NOSIGNAL) < 0) {
        return FALSE;
    }
    return TRUE;

}

Symbol** makeGrid () {

  int i, j;

  Symbol** grid = (Symbol**)malloc(GRIDSIZE * sizeof(Symbol*));
  for (i = 0; i < GRIDSIZE; i++) {
    grid[i] = malloc(GRIDSIZE * sizeof(Symbol));
  }

  for (i = 0; i < GRIDSIZE; i++) {
    for (j = 0; j < GRIDSIZE; j++) {
      strcpy(grid[i][j], "0");
    }
  }

  return grid;

}
void freeGrid (Symbol** grid) {

    int i;
    for (i = 0; i < GRIDSIZE; i++) {
        free(grid[i]);
    }
    free(grid);

}


// A OGNI DISCONNESSIONE BISOGNA SETTARE LA POSIZIONE DELL'ARRAY A NULL
void* gameThread (void* argument) {

    Game* this = (Game*) argument;

    printf("Game started. Serial: %d\n\n", this->gameSerial);

    int valread, valsend;
    char ack[2] = "";
    char client_buffer[20] = "";
    char move[2] = "";
    char outcome[2] = "";
    int winIsReached = FALSE;
    char server_msg[20] = "";
    int new_x, new_y;
    char x_str[3], y_str[3], new_x_str[3], new_y_str[3];
    int activePlayer_index = -1, defending_player, atk, def;
    char atk_str[2] = "", def_str[2] = "";
    char activePlayer_index_str[3] = "", defending_player_str[3] = "";
    int loopedOnce = FALSE;
    int i;

    while (!winIsReached) {

        pthread_mutex_lock(&playerLocks[this->gameSerial]);
        activePlayer_index++;
        if (activePlayer_index == NUMBER_OF_PLAYERS) {
            activePlayer_index = 0;
        }
        while (this->players[activePlayer_index] == NULL) {
            activePlayer_index++;
            if (activePlayer_index == NUMBER_OF_PLAYERS) {
                if (loopedOnce) {
                    printf("Hanno appeso tutti\n");
                    pthread_exit(NULL);
                }
                else {
                    activePlayer_index = 0;
                    loopedOnce = TRUE;
                }
            }
        }
        loopedOnce = FALSE;
        this->activePlayer = this->players[activePlayer_index];
        pthread_mutex_unlock(&playerLocks[this->gameSerial]);

        sprintf(activePlayer_index_str, "%d", activePlayer_index);

        strcat(server_msg, "1");                  // 1 per segnalare al client che sta venendo inviato il giocatore attivo
        strcat(server_msg, activePlayer_index_str);
        strcat(server_msg, "\n");

        // INVIO DEL GIOCATORE ATTIVO
        for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
            if (this->players[i]) {
                if ((valsend = send(this->players[i]->client->socket, server_msg, strlen(server_msg), MSG_NOSIGNAL)) < 0) {
                    printf("CRASH client %d\n", this->players[i]->client->socket);
                    free(this->players[i]);
                    this->players[i] = NULL;
                } printf("6 Giocatore attivo inviato al client %d\n", this->players[i]->client->socket);
                memset(ack, 0, 2);
            }
        }
        memset(server_msg, 0, sizeof(server_msg));

        printf("Il giocatore attivo %s e' in posizione (%d, %d)\n", this->players[activePlayer_index]->client->nickname, this->players[activePlayer_index]->x, this->players[activePlayer_index]->y);

        memset(move, 0, strlen(move));
        memset(client_buffer, 0, 20);
        // LETTURA DELLA MOSSA
        if (read(this->players[activePlayer_index]->client->pipe[0], client_buffer, sizeof(client_buffer)) <= 0) {
            printf("Errore di pipe dal client %d\n", this->players[activePlayer_index]->client->socket);
            pthread_exit(NULL);
        } printf("Mossa letta: %c\n", client_buffer[1]);

        if (client_buffer[0] == 'X') {
            this->players[activePlayer_index]->client->activeGame = -1;
            this->players[activePlayer_index]->client->positionInArrayOfPlayers = -1;
            this->players[activePlayer_index] = NULL;
            continue;
        }

        move[0] = client_buffer[1];

        switch (move[0]) {

            case 'N':
                new_x = this->players[activePlayer_index]->x;
                new_y = this->players[activePlayer_index]->y - 1;
            break;

            case 'S':
                new_x = this->players[activePlayer_index]->x;
                new_y = this->players[activePlayer_index]->y + 1;
            break;

            case 'O':
                new_x = this->players[activePlayer_index]->x - 1;
                new_y = this->players[activePlayer_index]->y;
            break;

            case 'E':
                new_x = this->players[activePlayer_index]->x + 1;
                new_y = this->players[activePlayer_index]->y;
            break;

            default:
                printf("Mossa non valida, errore di ricezione dal client %d\n", this->players[activePlayer_index]->client->socket);
                pthread_exit(NULL);
            break;

        }

        printf("%s vuole spostarsi in posizione (%d, %d)\n", this->players[activePlayer_index]->client->nickname, new_x, new_y);

        // CASELLA OCCUPATA
        if (strcmp(this->grid[new_x][new_y], "0") != 0 && strcmp(this->grid[new_x][new_y], this->players[activePlayer_index]->symbol) != 0) {
            defending_player = atoi(this->grid[new_x][new_y]) - 1;
            sprintf(defending_player_str, "%d", defending_player);
            if (this->players[defending_player]) {
                printf("La posizione (%d, %d) è occupata dal giocatore %s\n", new_x, new_y, this->players[defending_player]->client->nickname);
            }
            else {
                printf("La posizione (%d, %d) è occupata da un disertore\n", new_x, new_y);
            }
            atk = rand()%6 + 1;
            def = rand()%6 + 1;
            sprintf(atk_str, "%d", atk);
            sprintf(def_str, "%d", def);
            printf("ATK: %d DEF: %d\n", atk, def);
            // VITTORIA ATTACCO
            if (atk > def) {
                strcpy(this->grid[new_x][new_y], this->players[activePlayer_index]->symbol);
                this->players[activePlayer_index]->territories++;
                if (this->players[defending_player]) {
                    this->players[defending_player]->territories--;
                }
                this->players[activePlayer_index]->x = new_x;
                this->players[activePlayer_index]->y = new_y;
                sprintf(outcome, "%d", 2);
                if (this->players[defending_player]) {
                    printf("Vince l'attacco: %s si sposta in posizione (%d, %d). %s possiede ora %d territori e il giocatore %s ne possiede %d\n\n", this->players[activePlayer_index]->client->nickname, this->players[activePlayer_index]->x, this->players[activePlayer_index]->y, this->players[activePlayer_index]->client->nickname, this->players[activePlayer_index]->territories, this->players[defending_player]->client->nickname, this->players[defending_player]->territories);
                }
                else {
                    printf("Vince l'attacco: %s si sposta in posizione (%d, %d). %s possiede ora %d territori.\n\n", this->players[activePlayer_index]->client->nickname, this->players[activePlayer_index]->x, this->players[activePlayer_index]->y, this->players[activePlayer_index]->client->nickname, this->players[activePlayer_index]->territories);
                }
            }
            // VITTORIA DIFESA
            else {
                sprintf(outcome, "%d", 3);
                if (this->players[defending_player]) {
                    printf("Vince la difesa del giocatore %s, spostamento non effettuato\n\n", this->players[defending_player]->client->nickname);
                }
                else {
                    printf("Vince la difesa del disertore, spostamento non effettuato\n\n");
                }
            }
        }
        else {
            // SPOSTAMENTO IN CELLA GIA' POSSEDUTA
            if (strcmp(this->grid[new_x][new_y], this->players[activePlayer_index]->symbol) == 0) {
                sprintf(outcome, "%d", 4);
                this->players[activePlayer_index]->x = new_x;
                this->players[activePlayer_index]->y = new_y;
                printf("Posizione gia' posseduta: spostamento effettuato.\n");
            }
            // SPOSTAMENTO IN CELLA LIBERA
            else {
                sprintf(outcome, "%d", 5);
                strcpy(this->grid[new_x][new_y], this->players[activePlayer_index]->symbol);
                this->players[activePlayer_index]->territories++;
                this->players[activePlayer_index]->x = new_x;
                this->players[activePlayer_index]->y = new_y;
                printf("Posizione libera: spostamento effettuato. %s ora possiede %d territori\n\n", this->players[activePlayer_index]->client->nickname, this->players[activePlayer_index]->territories);
            }
        }

        strcat(server_msg, outcome);
        strcat(server_msg, move);
        if (strcmp(outcome, "2") == 0 || strcmp(outcome, "3") == 0) {
            strcat(server_msg, atk_str);
            strcat(server_msg, def_str);
            strcat(server_msg, defending_player_str);
        }
        strcat(server_msg, "\n");

        // INVIO ESITO MOSSA
        for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
            if (this->players[i]) {
                if ((valsend = send(this->players[i]->client->socket, server_msg, strlen(server_msg), MSG_NOSIGNAL)) < 0) {
                    printf("Errore di ricezione dal client %d durante l'invio esito\n", this->players[i]->client->socket);
                    pthread_exit(NULL);
                } printf("7 Esito mossa (%s) inviato al client %d\n", server_msg, this->players[i]->client->socket);
            }
        }
        memset(server_msg, 0, sizeof(server_msg));

        printf("Qui ci arriva 1\n");
        if (this->players[activePlayer_index]->territories == WIN) {
            printf("%s vince la partita\n\n", this->players[activePlayer_index]->client->nickname);
            winIsReached = TRUE;
        }
        printf("Qui ci arriva 2\n");

    }

    freeGrid(this->grid);
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        free(this->players[i]);
    }
    free(this);

}


void start_match () {

    Game* game = gameArgs[gamenum];

    printf("start_match invoked\n");

    Client* client;
    Player* player = NULL;

    pthread_mutex_lock(&lookingForMatch_lock);
    List* p = clientsLookingForMatch;

    int i = 0;
    for (p = clientsLookingForMatch; p; p = p->next) {
        client = p->data;
        client->activeGame = gamenum;
        client->positionInArrayOfPlayers = i;
        player = (Player*)malloc(sizeof(Player));
        player->client = client;
        game->players[i] = player;
        i++;
    }

    clientsLookingForMatch = freelist(clientsLookingForMatch, NULL);
    pthread_mutex_unlock(&lookingForMatch_lock);

    Symbol** grid = makeGrid();
    int winIsReached = FALSE;
    Player* players[NUMBER_OF_PLAYERS];
    int valsend, valread;
    int x, y;
    int new_x, new_y;
    char x_str[3], y_str[3], new_x_str[3], new_y_str[3];
    int activePlayer_index = 0, defending_player, atk, def;
    char atk_str[2] = "", def_str[2] = "";
    char activePlayer_index_str[3] = "", defending_player_str[3] = "";
    char ack[2] = "";
    char move[2] = "";
    char outcome[2] = "";
    char player_data[50] = "";


    int j;
    // INVIO MESSAGGIO DI INIZIO PARTITA
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if ((valsend = send(game->players[i]->client->socket, START_MATCH, 2, MSG_NOSIGNAL)) < 0) {
            printf("Errore di ricezione dal client %d\n", game->players[i]->client->socket);
            pthread_exit(NULL);
        } printf("Messaggio di partita iniziata inviato al client %d\n", game->players[i]->client->socket);

        memset(ack, 0, 2);
    }


    // POPOLAMENTO DELL'ARRAY DI GIOCATORI E INVIO DEI LORO DATI
    game->grid = grid;
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        strcat(player_data, game->players[i]->client->nickname);
        strcat(player_data, "|");
        snprintf(game->players[i]->symbol, SYMBOL_SIZE, "%d", i+1);
        strcat(player_data, game->players[i]->symbol);
        strcat(player_data, "|");
        game->players[i]->territories = 1;
        game->players[i]->isActive = FALSE;
        do {
            x = rand()%GRIDSIZE;
            y = rand()%GRIDSIZE;
        } while (!(strcmp(grid[x][y], "0") == 0));
        game->players[i]->x = x;
        game->players[i]->y = y;
        sprintf(x_str, "%d", x);
        sprintf(y_str, "%d", y);
        strcat(player_data, x_str);
        strcat(player_data, "|");
        strcat(player_data, y_str);
        strcat(player_data, "\n");
        strcpy(grid[x][y], game->players[i]->symbol);
        for (j = 0; j < NUMBER_OF_PLAYERS; j++) {
            if ((valsend = send(game->players[j]->client->socket, player_data, strlen(player_data), MSG_NOSIGNAL)) < 0) {
                printf("Errore di ricezione dal client %d\n", game->players[j]->client->socket);
                pthread_exit(NULL);
            } printf("Dati del giocatore %d inviati al client %d\n", i, game->players[j]->client->socket);
            memset(ack, 0, 2);
        }
        memset(player_data, 0, 50);
    }

    // INVIO TERMINAZIONE DEI DATI DEI GIOCATORI
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if ((valsend = send(game->players[i]->client->socket, "|\n", 2, MSG_NOSIGNAL)) < 0) {
            printf("Errore di ricezione dal client %d\n", game->players[i]->client->socket);
            pthread_exit(NULL);
        } printf("Terminatore dell'invio dei dati dei giocatori inviati al client %d\n", game->players[i]->client->socket);
        memset(ack, 0, 2);
    }

    pthread_create(&(gameThreads[gamenum++]), NULL, &gameThread, game);

}


void* clientThread (void* argument) {

    Client* this = (Client*)argument;
    this->thread = pthread_self();

    int p[2];
    memcpy(this->pipe, p, sizeof(this->pipe));
    if (pipe(this->pipe) < 0) {
        perror("Errore di pipe");
        exit(EXIT_FAILURE);
    };

    char client_buffer[BUFFER_LENGTH];

    int client_action;
    int signupResult, loginResult, sendlistResult;

    char ack[2] = "";
    int mutex_index;

    while (1) {

        memset(client_buffer, 0, strlen(client_buffer));
        if (read(this->socket, client_buffer, BUFFER_LENGTH) <= 0) {
            onClientDisconnection(this);
            close(this->socket);
            pthread_exit(NULL);
        }
        printf("MESSAGGIO RICEVUTO DAL CLIENT %d: %s\n", this->socket, client_buffer);

        client_action = client_buffer[0] - '0';   // Converte char a cifra singola nel rispettivo int

        switch (client_action) {

            case SIGNUP:
                strcat(client_buffer, "\n");
                pthread_mutex_lock(&signup_lock);
                signupResult = saveCredentialsToFile(client_buffer);
                pthread_mutex_unlock(&signup_lock);
                if (signupResult == TRUE) {
                    if (send(this->socket, SIGNUP_SUCCESS, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(this);
                        close(this->socket);
                        pthread_exit(NULL);
                    }
                    else {
                        printf("Il client %d registra un utente\n\n", this->socket);
                    }
                }
                else {
                    if (send(this->socket, USER_ALREADY_EXISTS, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(this);
                        close(this->socket);
                        pthread_exit(NULL);
                    }
                }
            break;

            case LOGIN:
                strcat(client_buffer, "\n");;
                pthread_mutex_lock(&login_lock);
                loginResult = logIn(client_buffer, this);
                printf("LOGIN RESULT: %d\n", loginResult);
                pthread_mutex_unlock(&login_lock);
                if (loginResult == 0) {
                    if (send(this->socket, LOGIN_SUCCESS, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(this);
                        close(this->socket);
                        pthread_exit(NULL);
                    }
                    else {
                        pthread_mutex_lock(&activeUsers_lock);
                        activeUsersList = push(activeUsersList, this->nickname);
                        pthread_mutex_unlock(&activeUsers_lock);
                        printf("%s si connette sul client %d\n\n", this->nickname, this->socket);
                    }
                }
                else if (loginResult == 1) {
                    memset(this->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(this->socket, USER_DOESNT_EXIST, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(this);
                        close(this->socket);
                        pthread_exit(NULL);
                    }
                }
                else {
                    memset(this->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(this->socket, WRONG_PASSWORD, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(this);
                        close(this->socket);
                        pthread_exit(NULL);
                    }
                }
            break;

            case VIEW_ACTIVE_USERS:
                pthread_mutex_lock(&activeUsers_lock);
                sendlistResult = sendUsersList(this);
                pthread_mutex_unlock(&activeUsers_lock);
                if (sendlistResult == FALSE) {
                    onClientDisconnection(this);
                    close(this->socket);
                    pthread_exit(NULL);
                }
            break;

            case LOOK_FOR_MATCH:
                pthread_mutex_lock(&lookingForMatch_lock);
                clientsLookingForMatch = append(clientsLookingForMatch, this);
                printf("CLIENT %d (%s) MESSO IN CODA\n", this->socket, this->nickname);
                pthread_mutex_unlock(&lookingForMatch_lock);
                if (length(clientsLookingForMatch) == NUMBER_OF_PLAYERS) {
                    pthread_mutex_lock(&gamenum_lock);
                    gameArgs[gamenum] = (Game*)malloc(sizeof(Game));
                    gameArgs[gamenum]->gameSerial = gamenum;
                    start_match();
                    pthread_mutex_unlock(&gamenum_lock);
                }
            break;

            case STOP_LOOKING_FOR_MATCH:
                pthread_mutex_lock(&lookingForMatch_lock);
                clientsLookingForMatch = delete(clientsLookingForMatch, this, areEqual_str, NULL);
                printf("CLIENT %d (%s) RIMOSSO DALLA CODA\n", this->socket, this->nickname);
                pthread_mutex_unlock(&lookingForMatch_lock);
                if (send(this->socket, LEAVE_QUEUE, 2, MSG_NOSIGNAL) < 0) {
                    onClientDisconnection(this);
                    close(this->socket);
                    pthread_exit(NULL);
                }

            break;

            case GAME_ACTION:
                write(this->pipe[1], client_buffer, strlen(client_buffer) + 1);
            break;

            case LEAVE_MATCH:
                printf("%s vuole abbandonare la partita %d\n", this->nickname, this->activeGame);
                if (send(this->socket, MATCH_LEFT, 2, MSG_NOSIGNAL) < 0) {
                    onClientDisconnection(this);
                    close(this->socket);
                }
                mutex_index = this->activeGame;
                pthread_mutex_lock(&playerLocks[mutex_index]);
                if (gameArgs[this->activeGame]->activePlayer->client == this) {
                    printf("%s era il giocatore attivo: invio del messaggio X\n", this->nickname);
                    write(this->pipe[1], "X", 2);
                }
                else {
                    gameArgs[this->activeGame]->players[this->positionInArrayOfPlayers] = NULL;
                    printf("Esso non era il giocatore attivo, viene messo a %p\n", gameArgs[this->activeGame]->players[this->positionInArrayOfPlayers]);
                    this->activeGame = -1;
                    this->positionInArrayOfPlayers = -1;
                }
                pthread_mutex_unlock(&playerLocks[mutex_index]);
            break;

            case LOGOUT:
                printf("%s effettua il logout dal client %d\n\n", this->nickname, this->socket);
                pthread_mutex_lock(&activeUsers_lock);
                activeUsersList = delete(activeUsersList, this->nickname, areEqual_str, NULL);
                pthread_mutex_unlock(&activeUsers_lock);
                memset(this->nickname, 0, MAX_CREDENTIALS_LENGTH);
            break;

            default:
                printf("DEFAULT\n");//send(this->socket, "DEFAULT\n", strlen("DEFAULT\n"), MSG_NOSIGNAL);   // da togliere
            break;

        }

    }

}



int main (int argc, char* argv[]) {

    //signal(SIGUSR1, start_match);

    srand(time(NULL));

    pthread_t clientThreads[40];
    int clientnum = 0;

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int options = 1;

    int listener_socket_fd = 0, new_socket = 0;

    Client* arg;



    int i;
    for (i = 0; i < 40; i++) {
        pthread_mutex_init(&playerLocks[i], NULL);
    }



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
            arg = (Client*)malloc(sizeof(Client));
            arg->socket = new_socket;
            arg->clientSerial = clientnum;
            pthread_create(&(clientThreads[clientnum++]), NULL, &clientThread, arg);
        }

    }

    exit(0);
}
