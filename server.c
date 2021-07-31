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

#define TRUE 1
#define FALSE 0

#define PORT 50000

#define MAX_CREDENTIALS_LENGTH 64
#define BUFFER_LENGTH 256
#define SYMBOL_SIZE 3

#define USERS_FILE "utenti.txt"
#define USER_EVENTS_FILE "autenticazioni.txt"

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
#define ACTIVE_PLAYER_AND_TIME "1"
#define SUCCESSFUL_ATTACK "2"
#define FAILED_ATTACK "3"
#define MOVE_ON_OWN_SQUARE "4"
#define MOVE_ON_FREE_SQUARE "5"
#define TIME_ENDED "6\n"
#define TIME_ENDEND_PIPE_MESSAGE_WRITE "T"
#define TIME_ENDED_PIPE_MESSAGE_READ 'T'

#define LEAVE_MATCH 7
#define MATCH_LEFT "0\n"
#define MATCH_LEFT_PIPE_MESSAGE_WRITE "X"
#define MATCH_LEFT_PIPE_MESSAGE_READ 'X'

#define LOGOUT 8

#define NUMBER_OF_PLAYERS 3
#define TIMER_SECONDS 30
#define GRIDSIZE 7
#define WIN 5

typedef char Symbol[SYMBOL_SIZE];

typedef struct Client {
    int id;
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
} Player;

typedef struct Game {
    pthread_t thread;
    int id;
    Symbol** grid;
    Player* players[NUMBER_OF_PLAYERS];
    Player* activePlayer;
} Game;


static const char* Weekdays[] = {"Domenica", "Lunedì", "Martedì", "Mercoledì", "Giovedì", "Venerdì", "Sabato"};


void makeTimestamp(char* timestamp) {

    time_t seconds;
    struct tm* date;
    char dateString[32];

    seconds = time(NULL);
    date = localtime(&seconds);

    sprintf(timestamp, "%s, %02d-%02d-%d %02d:%02d:%02d",
        Weekdays[date->tm_wday],
        date->tm_mday,
        date->tm_mon + 1,
        date->tm_year + 1900,
        date->tm_hour,
        date->tm_min,
        date->tm_sec);

}

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
int user_events_fd;
int gamenum = 0;
List* activeUsersList = NULL;
List* clientsLookingForMatch = NULL;
Game* gameArgs[40];
pthread_t gameThreads[40];
pthread_mutex_t activePlayerLocks[40];
pthread_mutex_t gameFileLocks[40];
pthread_mutex_t gamenum_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t users_file_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t user_events_file_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t activeUsers_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lookingForMatch_lock = PTHREAD_MUTEX_INITIALIZER;


void log_ClientConnection (int client, struct in_addr ip) {

    char log[BUFFER_LENGTH] = "";
    char timestamp[32];
    char ip_str[INET_ADDRSTRLEN];

    makeTimestamp(timestamp);
    inet_ntop(AF_INET, &ip, ip_str, INET_ADDRSTRLEN);

    snprintf(log, sizeof(log), "[%s] CONNESSIONE CLIENT: si connette il client %d con indirizzo %s\n\n", timestamp, client, ip_str);

    pthread_mutex_lock(&user_events_file_lock);
    if (write(user_events_fd, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file degli eventi degli utenti");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&user_events_file_lock);

}
void log_SignUp (char* nickname, int client) {

    char log[BUFFER_LENGTH] = "";
    char timestamp[32] = "";

    makeTimestamp(timestamp);

    snprintf(log, sizeof(log), "[%s] REGISTRAZIONE UTENTE: %s si registra al servizio dal clent %d\n\n", timestamp, nickname, client);

    pthread_mutex_lock(&user_events_file_lock);
    if (write(user_events_fd, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file degli eventi degli utenti");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&user_events_file_lock);

}
void log_SignIn (char* nickname, int client) {

    char log[BUFFER_LENGTH] = "";
    char timestamp[32] = "";

    makeTimestamp(timestamp);
    snprintf(log, sizeof(log), "[%s] ACCESSO UTENTE: %s si registra al servizio dal client %d\n\n", timestamp, nickname, client);

    pthread_mutex_lock(&user_events_file_lock);
    if (write(user_events_fd, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file degli eventi degli utenti");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&user_events_file_lock);

}
void log_SignOut (char* nickname, int client) {

    char log[BUFFER_LENGTH] = "";
    char timestamp[32] = "";

    makeTimestamp(timestamp);
    snprintf(log, sizeof(log), "[%s] LOG OUT UTENTE: %s si disconnette dal client %d\n\n", timestamp, nickname, client);

    pthread_mutex_lock(&user_events_file_lock);
    if (write(user_events_fd, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file degli eventi degli utenti");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&user_events_file_lock);

}
int log_GameStart (Game* game) {

    int gameFile;
    char gameFileName[64];

    char timestamp[32] = "";
    char log[1024];
    char player_data [512] = "";

    int i;
    char i_str[8];

    makeTimestamp(timestamp);

    snprintf(gameFileName, sizeof(gameFileName), "Partita di %s", timestamp);

    snprintf(log, sizeof(log), "------ PARTITA INIZIATA [%s] ------\n\nGiocatori: ", timestamp);

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        strcat(player_data, game->players[i]->client->nickname);
        snprintf(i_str, sizeof(i_str), " (%d)", i);
        strcat(player_data, i_str);
        if (i < NUMBER_OF_PLAYERS - 1) {
            strcat(player_data, ", ");
        }
    }
    strcat(player_data, "\n\n\n");

    strcat(log, player_data);

    gameFile = open(gameFileName, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if (gameFile == -1) {
      perror("Errore di apertura del file della partita");
      exit(-1);
    }

    if (write(gameFile, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file della partita");
        exit(EXIT_FAILURE);
    }

    return gameFile;

}
void log_GameConquest (int gameFile, Player* activePlayer, Player* defendingPlayer, int gameID) {

    char log[512] = "";
    char timestamp[32] = "";

    makeTimestamp(timestamp);

    pthread_mutex_lock(&activePlayerLocks[gameID]);
    if (defendingPlayer) {
        snprintf(log, sizeof(log), "[%s] CONQUISTA TERRITORIO: %s si sposta in posizione (%d, %d), sottraendola a %s. Territori di %s: %d, Territori di %s: %d\n\n",
            timestamp,
            activePlayer->client->nickname,
            activePlayer->x,
            activePlayer->y,
            defendingPlayer->client->nickname,
            activePlayer->client->nickname,
            activePlayer->territories,
            defendingPlayer->client->nickname,
            defendingPlayer->territories);
    }
    else {
        snprintf(log, sizeof(log), "[%s] CONQUISTA TERRITORIO: %s si sposta in posizione (%d, %d). Territori di %s: %d\n\n",
            timestamp,
            activePlayer->client->nickname,
            activePlayer->x,
            activePlayer->y,
            activePlayer->client->nickname,
            activePlayer->territories);
    }
    pthread_mutex_unlock(&activePlayerLocks[gameID]);

    pthread_mutex_lock(&gameFileLocks[gameID]);
    if (write(gameFile, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file della partita");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&gameFileLocks[gameID]);

}
void log_GameEnd (int gameFile, char* winner) {

    char timestamp[32] = "";
    char log[512];

    makeTimestamp(timestamp);
    if (winner) {
        snprintf(log, sizeof(log), "[%s] VITTORIA: %s vince la partita\n\n\n------ PARTITA FINITA [%s] ------\n", timestamp, winner, timestamp);
    }
    else {
        snprintf(log, sizeof(log), "\n\n------ PARTITA FINITA PER ABBANDONO [%s] ------\n", timestamp);
    }

    if (write(gameFile, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file della partita");
        exit(EXIT_FAILURE);
    }

    close(gameFile);

}
void log_ClientDisconnection (int client) {

    char log[BUFFER_LENGTH] = "";
    char timestamp[32] = "";

    makeTimestamp(timestamp);
    snprintf(log, sizeof(log), "[%s] DISCONNESSIONE: Il client %d si disconnette\n\n", timestamp, client);

    pthread_mutex_lock(&user_events_file_lock);
    if (write(user_events_fd, log, strlen(log)) == -1) {
        perror("Errore di scrittura sul file degli eventi degli utenti");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_unlock(&user_events_file_lock);

}


int userExists (char* nickname) {

    // NON SI LOCKA IL MUTEX QUI PERCHE' VIENE LOCKATO DALLA FUNZIONE CHIAMANTE

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
int saveCredentialsToFile (char* credentials_buffer, int socket) {

    strcat(credentials_buffer, "\n");

    char log[BUFFER_LENGTH] = "";
    char timestamp[32] = "";

    char credentials[MAX_CREDENTIALS_LENGTH] = {0};
    char nickname[MAX_CREDENTIALS_LENGTH] = {0};

    int i, nick_flag = TRUE, ret = TRUE;

    pthread_mutex_lock(&users_file_lock);

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
        ret = FALSE;
    }

    if (write(users_fd, credentials, strlen(credentials)) == -1) {
        perror("Errore di scrittura sul file degli utenti");
        exit(EXIT_FAILURE);
    }

    log_SignUp(nickname, socket);

    pthread_mutex_unlock(&users_file_lock);

    return ret;

}
int signIn (char* credentials_buffer, Client* client) {

    strcat(credentials_buffer, "\n");;

    char read_char[1];
    char nickname[MAX_CREDENTIALS_LENGTH];
    char password[MAX_CREDENTIALS_LENGTH];
    char nick_to_compare[MAX_CREDENTIALS_LENGTH];
    char password_to_compare[MAX_CREDENTIALS_LENGTH];

    int i, j = 0, nick_flag = TRUE;
    for (i = 1; credentials_buffer[i] != '\n'; i++) {
        if (credentials_buffer[i] == '|') {
            nickname[i-1] = '\0';
            client->nickname[i-1] = '\0';
            nick_flag = FALSE;
        }
        else {
            if (nick_flag) {
                nickname[i-1] = credentials_buffer[i];
                client->nickname[i-1] = credentials_buffer[i];
            }
            else {
                password[j] = credentials_buffer[i];
                j++;
            }
        }
    }
    password[i] = '\0';

    i = 0; j = 0;

    pthread_mutex_lock(&users_file_lock);

    if (lseek(users_fd, 0, SEEK_SET) < 0) {
        perror("Errore di seek");
        exit(EXIT_FAILURE);
    }

    int nickname_found = FALSE, password_reached = FALSE, ret = 1;
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
                    ret = 0;   // LOGIN SUCCESS
                    break;
                }
                else {
                    ret = 2;   // WRONG PASSWORD
                    break;
                }
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

    pthread_mutex_unlock(&users_file_lock);

    return ret;    // USER NOT FOUND (DOESN'T EXIST)

}
void onClientDisconnection (Client* client) {

    int mutex_index = client->activeGame;

    printf("Client %d disconnesso\n", client->socket);

    pthread_mutex_lock(&activeUsers_lock);
    activeUsersList = delete(activeUsersList, client->nickname, areEqual_str, NULL);
    pthread_mutex_unlock(&activeUsers_lock);

    pthread_mutex_lock(&lookingForMatch_lock);
    clientsLookingForMatch = delete(clientsLookingForMatch, client, areEqual_cli, NULL);
    pthread_mutex_unlock(&lookingForMatch_lock);

    if (mutex_index >= 0) {

        pthread_mutex_lock(&activePlayerLocks[mutex_index]);
        if (gameArgs[client->activeGame]->activePlayer->client == client) {
            if (write(client->pipe[1], MATCH_LEFT_PIPE_MESSAGE_WRITE, strlen(MATCH_LEFT_PIPE_MESSAGE_WRITE)) < 0) {
                perror("Errore di scrittura sulla pipe");
                exit(EXIT_FAILURE);
            };
        }
        else {
            gameArgs[client->activeGame]->players[client->positionInArrayOfPlayers] = NULL;
        }
        pthread_mutex_unlock(&activePlayerLocks[mutex_index]);

    }

    log_ClientDisconnection(client->socket);

    memset(client->nickname, 0, MAX_CREDENTIALS_LENGTH);

    close(client->socket);

    free(client);

    pthread_exit(NULL);

}
void sendUsersList (Client* client) {

    pthread_mutex_lock(&activeUsers_lock);

    List* p = activeUsersList;
    char* nickname;
    int nick_size;

    while (p) {

        nick_size = strlen((char*)(p->data)) + 1;

        nickname = (char*)malloc(nick_size * sizeof(char));

        strcpy(nickname, (char*)(p->data));
        strcat(nickname, "\n");

        if (send(client->socket, nickname, nick_size, MSG_NOSIGNAL) < 0) {
            onClientDisconnection(client);
        }

        free(nickname);
        p = p->next;

    }
    if (send(client->socket, "|\n", 2, MSG_NOSIGNAL) < 0) {
        onClientDisconnection(client);
    }

    pthread_mutex_unlock(&activeUsers_lock);

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



void* timerThread (void* argument) {

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    Game* game = argument;
    Player* activePlayer = game->activePlayer;

    sleep(TIMER_SECONDS);

    pthread_mutex_lock(&activePlayerLocks[game->id]);
    if (game->activePlayer == activePlayer) {
        if (write(game->activePlayer->client->pipe[1], TIME_ENDEND_PIPE_MESSAGE_WRITE, strlen(TIME_ENDEND_PIPE_MESSAGE_WRITE)) < 0) {
            perror("Errore di scrittura sulla pipe");
            exit(EXIT_FAILURE);
        };
    }
    pthread_mutex_unlock(&activePlayerLocks[game->id]);

}
void setNewActivePlayer (Game* game, int* activePlayerIndex, int gameFile) {

    pthread_mutex_lock(&activePlayerLocks[game->id]);

    int loopedOnce = FALSE;
    int i;

    *activePlayerIndex += 1;

    if (*activePlayerIndex == NUMBER_OF_PLAYERS) {
        *activePlayerIndex = 0;
    }

    while (!game->players[*activePlayerIndex]) {
        *activePlayerIndex += 1;
        if (*activePlayerIndex == NUMBER_OF_PLAYERS) {
            if (loopedOnce) {
                log_GameEnd(gameFile, NULL);
                freeGrid(game->grid);
                for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                    if (game->players[i]) {
                        game->players[i]->client->activeGame = -1;
                        game->players[i]->client->positionInArrayOfPlayers = -1;
                    }
                    free(game->players[i]);
                }
                free(game);
                pthread_exit(NULL);
            }
            else {
                *activePlayerIndex = 0;
                loopedOnce = TRUE;
            }
        }
    }

    game->activePlayer = game->players[*activePlayerIndex];

    pthread_mutex_unlock(&activePlayerLocks[game->id]);

}
void sendActivePlayerAndTimerEnd (Game* game, int activePlayerIndex) {

    // Il messaggio è della forma 1 + (0) + INDICE GIOCATORE + TEMPO DI FINE + \n

    unsigned long timerEnd = time(NULL) + TIMER_SECONDS;

    char activePlayerIndex_str[2];
    char timerEnd_str[16];
    char message[32] = "";

    int i;

    snprintf(activePlayerIndex_str, sizeof(activePlayerIndex_str), "%d", activePlayerIndex);
    snprintf(timerEnd_str, sizeof(timerEnd_str), "%lu", timerEnd);

    strcat(message, ACTIVE_PLAYER_AND_TIME);
    if (activePlayerIndex >= 0 && activePlayerIndex <= 9) {
        strcat(message, "0");
    }
    strcat(message, activePlayerIndex_str);
    strcat(message, timerEnd_str);
    strcat(message, "\n");

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (game->players[i]) {
            if (send(game->players[i]->client->socket, message, strlen(message), MSG_NOSIGNAL) < 0) {
                free(game->players[i]);
                game->players[i] = NULL;
            }
        }
    }

    printf("Il giocatore attivo %s e' in posizione (%d, %d)\n", game->activePlayer->client->nickname, game->activePlayer->x, game->activePlayer->y);
    printf("Messaggio inviato: %s\n", message);

}
void handleMoveTimeout (Game* game) {

    int i;

    printf("\nTempo scaduto per %s\n", game->activePlayer->client->nickname);
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (game->players[i]) {
            if (send(game->players[i]->client->socket, TIME_ENDED, strlen(TIME_ENDED), MSG_NOSIGNAL) < 0) {
                free(game->players[i]);
                game->players[i] = NULL;
            }
        }
    }

}
void handleMatchLeftFromActivePlayer (Game* game, int activePlayerIndex) {
    game->activePlayer->client->activeGame = -1;
    game->activePlayer->client->positionInArrayOfPlayers = -1;
    free(game->players[activePlayerIndex]);
    game->players[activePlayerIndex] = NULL;
    game->activePlayer = NULL;
}
int squareIsOwnedByEnemy (Game* game, int x, int y) {
    return (strcmp(game->grid[x][y], "0") != 0 && strcmp(game->grid[x][y], game->activePlayer->symbol) != 0);
}
void handleSquareIsOwnedByEnemy (Game* game, int x, int y, char moveToSend, int gameFile) {

    int defendingPlayerIndex = atoi(game->grid[x][y]) - 1;
    int atk = rand()%6 + 1;
    int def = rand()%6 + 1;
    int i;
    char outcome[2];
    char message[16];

    if (atk > def) {

        strcpy(outcome, SUCCESSFUL_ATTACK);

        strcpy(game->grid[x][y], game->activePlayer->symbol);
        game->activePlayer->territories++;
        if (game->players[defendingPlayerIndex]) {
            game->players[defendingPlayerIndex]->territories--;
        }
        game->activePlayer->x = x;
        game->activePlayer->y = y;

        log_GameConquest(gameFile, game->activePlayer, game->players[defendingPlayerIndex], game->id);

    }
    else {
        strcpy(outcome, FAILED_ATTACK);
    }

    snprintf(message, sizeof(message), "%s%c%d%d%d\n", outcome, moveToSend, atk, def, defendingPlayerIndex);

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (game->players[i]) {
            if (send(game->players[i]->client->socket, message, strlen(message), MSG_NOSIGNAL) < 0) {
                free(game->players[i]);
                game->players[i] = NULL;
            }
        }
    }

}
int squareIsOwnedBySelf (Game* game, int x, int y) {
    return (strcmp(game->grid[x][y], game->activePlayer->symbol) == 0);
}
void handleSquareIsOwnedBySelf (Game* game, int x, int y, char moveToSend) {

    int i;
    char message[8];

    game->activePlayer->x = x;
    game->activePlayer->y = y;

    snprintf(message, sizeof(message), "%s%c\n", MOVE_ON_OWN_SQUARE, moveToSend);

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (game->players[i]) {
            if (send(game->players[i]->client->socket, message, strlen(message), MSG_NOSIGNAL) < 0) {
                free(game->players[i]);
                game->players[i] = NULL;
            }
        }
    }


}
void handleSquareIsFree (Game* game, int x, int y, char moveToSend, int gameFile) {

    int i;

    char message[8];

    strcpy(game->grid[x][y], game->activePlayer->symbol);
    game->activePlayer->territories++;
    game->activePlayer->x = x;
    game->activePlayer->y = y;

    log_GameConquest(gameFile, game->activePlayer, NULL, game->id);

    snprintf(message, sizeof(message), "%s%c\n", MOVE_ON_FREE_SQUARE, moveToSend);

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (game->players[i]) {
            if (send(game->players[i]->client->socket, message, strlen(message), MSG_NOSIGNAL) < 0) {
                free(game->players[i]);
                game->players[i] = NULL;
            }
        }
    }


}
void* gameThread (void* game) {

    Game* this = game;
    this->thread = pthread_self();

    int gameFile = log_GameStart(this);

    int winIsReached = FALSE;

    pthread_t timer;

    char clientMessage[1];
    char gameAction;
    char move;

    int activePlayerIndex = -1;
    int defendingPlayerIndex;
    int new_x, new_y;

    int i;

    while (!winIsReached) {

        setNewActivePlayer(this, &activePlayerIndex, gameFile);

        sendActivePlayerAndTimerEnd(this, activePlayerIndex);

        pthread_create(&timer, NULL, timerThread, this);

        if (read(this->activePlayer->client->pipe[0], clientMessage, sizeof(clientMessage)) < 0) {
            perror("Errore di lettura dalla pipe");
            exit(EXIT_FAILURE);
        }

        pthread_cancel(timer);

        gameAction = clientMessage[0];
        switch (gameAction) {

            case 'N':
                new_x = this->activePlayer->x;
                new_y = this->activePlayer->y - 1;
            break;

            case 'S':
                new_x = this->activePlayer->x;
                new_y = this->activePlayer->y + 1;
            break;

            case 'O':
                new_x = this->activePlayer->x - 1;
                new_y = this->activePlayer->y;
            break;

            case 'E':
                new_x = this->activePlayer->x + 1;
                new_y = this->activePlayer->y;
            break;

            case TIME_ENDED_PIPE_MESSAGE_READ:
                handleMoveTimeout(this);
                continue;
            break;

            case MATCH_LEFT_PIPE_MESSAGE_READ:
                handleMatchLeftFromActivePlayer(this, activePlayerIndex);
                continue;
            break;

        }
        move = gameAction;

        if (squareIsOwnedByEnemy(this, new_x, new_y)) {
            handleSquareIsOwnedByEnemy(this, new_x, new_y, move, gameFile);
        }
        else {
            if (squareIsOwnedBySelf(this, new_x, new_y)) {
                handleSquareIsOwnedBySelf(this, new_x, new_y, move);
            }
            else {
                handleSquareIsFree(this, new_x, new_y, move, gameFile);
            }
        }

        if (this->activePlayer->territories == WIN) {
            printf("%s vince la partita\n\n", this->activePlayer->client->nickname);
            log_GameEnd(gameFile, this->activePlayer->client->nickname);
            winIsReached = TRUE;
        }

        memset(clientMessage, 0, strlen(clientMessage));

    }

    freeGrid(this->grid);
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (this->players[i]) {
            this->players[i]->client->activeGame = -1;
            this->players[i]->client->positionInArrayOfPlayers = -1;
        }
        free(this->players[i]);
    }
    free(this);

}



void makePlayers (Game* game) {

    List* p;
    Client* client;
    Player* player;
    int i = 0;

    pthread_mutex_lock(&lookingForMatch_lock);

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

}
void notifyPlayersOfStartMatch (Game* game) {

    int i;

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (send(game->players[i]->client->socket, START_MATCH, strlen(START_MATCH), MSG_NOSIGNAL) < 0) {
            free(game->players[i]);
            game->players[i] = NULL;
        }
    }

}
void sendDataToAllPlayers (Game* game, char* data) {

    int i;

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (send(game->players[i]->client->socket, data, strlen(data), MSG_NOSIGNAL) < 0) {
            free(game->players[i]);
            game->players[i] = NULL;
        }
    }

}
void initGame (Game* game) {

    Symbol** grid = makeGrid();

    int x, y;

    char playerData[128];

    int i;


    game->grid = grid;

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {

        snprintf(game->players[i]->symbol, SYMBOL_SIZE, "%d", i+1);

        game->players[i]->territories = 1;
        do {
            x = rand()%GRIDSIZE;
            y = rand()%GRIDSIZE;
        } while (!(strcmp(grid[x][y], "0") == 0));
        game->players[i]->x = x;
        game->players[i]->y = y;

        strcpy(grid[x][y], game->players[i]->symbol);

        snprintf(playerData, sizeof(playerData), "%s|%s|%d|%d\n", game->players[i]->client->nickname, game->players[i]->symbol, x, y);

        sendDataToAllPlayers(game, playerData);

    }

    // Invia il terminatore "|\n" per segnalare ai giocatori che è terminato l'invio dei dati sui giocatori
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (send(game->players[i]->client->socket, "|\n", 2, MSG_NOSIGNAL) < 0) {
            free(game->players[i]);
            game->players[i] = NULL;
        }
    }

}
void start_match () {

    pthread_mutex_lock(&gamenum_lock);

    Game* game = (Game*)malloc(sizeof(Game));
    game->id = gamenum;

    makePlayers(game);

    notifyPlayersOfStartMatch(game);

    initGame(game);

    gameArgs[gamenum] = game;

    pthread_create(&(gameThreads[gamenum++]), NULL, gameThread, game);

    pthread_mutex_unlock(&gamenum_lock);

}



Client* createClient (int id, int socket) {

    Client* client;
    struct timeval tv;
    int p[2];

    client = (Client*)malloc(sizeof(Client));
    client->id = id;
    client->socket = socket;
    client->activeGame = -1;
    client->positionInArrayOfPlayers = -1;

    memcpy(client->pipe, p, sizeof(client->pipe));
    if (pipe(client->pipe) < 0) {
        perror("Errore di pipe");
        exit(EXIT_FAILURE);
    };

    tv.tv_sec = 330;
    tv.tv_usec = 0;

    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv)) {
        perror("Errore nell'impostazione del timeout della socket");
        exit(EXIT_FAILURE);
    };

    return client;

}
void handleSignUpResult (Client* client, int signUpResult) {

    if (signUpResult == TRUE) {
        if (send(client->socket, SIGNUP_SUCCESS, 2, MSG_NOSIGNAL) < 0) {
            onClientDisconnection(client);
        }
    }
    else {
        if (send(client->socket, USER_ALREADY_EXISTS, 2, MSG_NOSIGNAL) < 0) {
            onClientDisconnection(client);
        }
    }

}
void handleSignInResult (Client* client, int signInResult) {

    if (signInResult == 0) {
        if (send(client->socket, LOGIN_SUCCESS, 2, MSG_NOSIGNAL) < 0) {
            onClientDisconnection(client);
        }

        pthread_mutex_lock(&activeUsers_lock);
        activeUsersList = push(activeUsersList, client->nickname);
        pthread_mutex_unlock(&activeUsers_lock);

        printf("%s si connette sul client %d\n\n", client->nickname, client->socket);
        log_SignIn(client->nickname, client->socket);
    }
    else if (signInResult == 1) {
        memset(client->nickname, 0, sizeof(client->nickname));
        if (send(client->socket, USER_DOESNT_EXIST, 2, MSG_NOSIGNAL) < 0) {
            onClientDisconnection(client);
        }
    }
    else {
        memset(client->nickname, 0, sizeof(client->nickname));
        if (send(client->socket, WRONG_PASSWORD, 2, MSG_NOSIGNAL) < 0) {
            onClientDisconnection(client);
        }
    }

}
void handleLeaveMatch (Client* client) {

    int mutex_index = client->activeGame;

    pthread_mutex_lock(&activePlayerLocks[mutex_index]);
    if (gameArgs[client->activeGame]->activePlayer->client == client) {
        if (write(client->pipe[1], MATCH_LEFT_PIPE_MESSAGE_WRITE, strlen(MATCH_LEFT_PIPE_MESSAGE_WRITE)) < 0) {
            perror("Errore di scrittura sulla pipe");
            exit(EXIT_FAILURE);
        }
    }
    else {
        free(gameArgs[client->activeGame]->players[client->positionInArrayOfPlayers]);
        gameArgs[client->activeGame]->players[client->positionInArrayOfPlayers] = NULL;
        client->activeGame = -1;
        client->positionInArrayOfPlayers = -1;
    }
    pthread_mutex_unlock(&activePlayerLocks[mutex_index]);

}
void* clientThread (void* client) {

    Client* this = client;
    this->thread = pthread_self();

    char clientMessage[64];

    int action;
    int signUpResult, signInResult;
    int mutex_index;


    while (TRUE) {

        if (read(this->socket, clientMessage, sizeof(clientMessage)) <= 0) {
            onClientDisconnection(this);
        }
        printf("MESSAGGIO RICEVUTO DAL CLIENT %d: %s\n", this->socket, clientMessage);

        action = clientMessage[0] - '0';   // Converte char a cifra singola nel rispettivo int

        switch (action) {

            case SIGNUP:
                signUpResult = saveCredentialsToFile(clientMessage, this->socket);
                handleSignUpResult(this, signUpResult);
            break;

            case LOGIN:
                signInResult = signIn(clientMessage, this);
                handleSignInResult(this, signInResult);
            break;

            case VIEW_ACTIVE_USERS:
                sendUsersList(this);
            break;

            case LOOK_FOR_MATCH:
                pthread_mutex_lock(&lookingForMatch_lock);
                clientsLookingForMatch = append(clientsLookingForMatch, this);
                pthread_mutex_unlock(&lookingForMatch_lock);

                if (length(clientsLookingForMatch) == NUMBER_OF_PLAYERS) {
                    start_match();
                }
            break;

            case STOP_LOOKING_FOR_MATCH:
                pthread_mutex_lock(&lookingForMatch_lock);
                clientsLookingForMatch = delete(clientsLookingForMatch, this, areEqual_str, NULL);
                pthread_mutex_unlock(&lookingForMatch_lock);

                if (send(this->socket, LEAVE_QUEUE, 2, MSG_NOSIGNAL) < 0) {
                    onClientDisconnection(this);
                }

            break;

            case GAME_ACTION:
                snprintf(clientMessage, sizeof(clientMessage), "%c", clientMessage[1]);
                if (write(this->pipe[1], clientMessage, strlen(clientMessage)) < 0) {
                    perror("Errore di scrittura sulla pipe");
                    exit(EXIT_FAILURE);
                }
            break;

            case LEAVE_MATCH:
                if (send(this->socket, MATCH_LEFT, 2, MSG_NOSIGNAL) < 0) {
                    onClientDisconnection(this);
                }
                handleLeaveMatch(this);
            break;

            case LOGOUT:
                pthread_mutex_lock(&activeUsers_lock);
                activeUsersList = delete(activeUsersList, this->nickname, areEqual_str, NULL);
                pthread_mutex_unlock(&activeUsers_lock);
                log_SignOut(this->nickname, this->socket);
                memset(this->nickname, 0, MAX_CREDENTIALS_LENGTH);
            break;

            default:
                printf("DEFAULT\n");
                // Nessuna send: il client si blocca qui
            break;

        }

        memset(clientMessage, 0, strlen(clientMessage));

    }

}



int main (int argc, char* argv[]) {

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
        pthread_mutex_init(&activePlayerLocks[i], NULL);
    }



    users_fd = open(USERS_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if (users_fd == -1) {
      perror("Errore di apertura del file degli utenti");
      exit(-1);
    }


    user_events_fd = open(USER_EVENTS_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if (user_events_fd == -1) {
      perror("Errore di apertura del file degli eventi degli utenti");
      exit(-1);
    }



    if ((listener_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Creazione della socket fallita");
        exit(EXIT_FAILURE);
    }
    printf("Socket di ascolto creata...\n");

    if (setsockopt(listener_socket_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &options, sizeof(options))) {
        perror("Errore durante l'impostazione delle opzioni della socket");
        exit(EXIT_FAILURE);
    }
    printf("Opzioni socket impostate...\n");

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(listener_socket_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("Bind fallito");
        exit(EXIT_FAILURE);
    }
    printf("Bound alla porta %d effettuato...\n", PORT);

    if (listen(listener_socket_fd, 50) < 0) {
        perror("Listen fallito");
        exit(EXIT_FAILURE);
    }
    printf("In ascolto...\n\n");



    while(1) {

        new_socket = accept(listener_socket_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("Errore di accept");
            exit(EXIT_FAILURE);
        }
        else {
            printf("\nClient found.\n\n");
            arg = createClient(clientnum, new_socket);

            log_ClientConnection(new_socket, address.sin_addr);

            pthread_create(&(clientThreads[clientnum++]), NULL, clientThread, arg);
        }

    }

    exit(0);
}
