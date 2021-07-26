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
#define SIGNUP_SUCCESS = "0\n";
#define USER_ALREADY_EXISTS = "1\n";

#define LOGIN 2
#define LOGIN_SUCCESS = "0\n"
#define USER_DOESNT_EXIST = "1\n"
#define WRONG_PASSWORD = "2\n"

#define VIEW_ACTIVE_USERS 3
#define LOOK_FOR_MATCH 4
#define LOGOUT 5
#define STOP_LOOKING_FOR_MATCH 6

#define NUMBER_OF_PLAYERS 2

typedef char Symbol[SYMBOL_SIZE];

typedef struct ClientThreadArg {
    int serial;
    char nickname[MAX_CREDENTIALS_LENGTH];
    int socket;
    pthread_t thread;
} ClientThreadArg;

typedef struct Player {
    int serial;
    char nickname[MAX_CREDENTIALS_LENGTH];
    int socket;
    pthread_t thread;
    Symbol symbol;
    int territories;
    int x;
    int y;
    int isActive;
} Player;

typedef struct GameThreadArg {
    Player* players[NUMBER_OF_PLAYERS];
} GameThreadArg;

int areEqual_cli (void* p1, void* p2) {
    ClientThreadArg* arg1 = (ClientThreadArg*)p1;
    ClientThreadArg* arg2 = (ClientThreadArg*)p2;

    return strcmp(arg1->nickname, arg2->nickname) == 0;
}

void printNode_cli (List* node) {
    ClientThreadArg* data = (ClientThreadArg*)(node->data);
    if (node->next) {
        printf("(%s, %d, %lu) -> ", data->nickname, data->socket, data->thread);
    }
    else {
        printf("(%s, %d, %lu) -> NULL\n", data->nickname, data->socket, data->thread);
    }
}

pthread_t main_thread;
pthread_t matchThread_id[40];
pthread_cond_t wake_conditions[40];
pthread_mutex_t condition_mutex[40];
int can_be_woken[40];   // A quanto pare servono queste variabili oltre alle condizioni per prevenire spurious wake ups. Se servono altri 40 mutex non lo so
int users_fd;
List* activeUsersList = NULL;
List* usersLookingForMatch = NULL;
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
int logIn (char credentials_buffer[MAX_CREDENTIALS_LENGTH], ClientThreadArg* clientArg) {

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
                    return 0;   // LOGIN SUCCESS
                }
                else
                    return 1;   // WRONG PASSWORD
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

    return 2;    // USER NOT FOUND (DOESN'T EXIST)

}
void onClientDisconnection (ClientThreadArg* clientArg) {

    printf("Errore di ricezione dal client %d: client disconnesso\n", clientArg->socket);

    pthread_mutex_lock(&activeUsers_lock);
    activeUsersList = delete(activeUsersList, clientArg->nickname, areEqual_str);
    pthread_mutex_unlock(&activeUsers_lock);

    memset(clientArg->nickname, 0, MAX_CREDENTIALS_LENGTH);

}
int sendUsersList (ClientThreadArg* clientArg) {

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

Symbol** makeGrid (int gridsize) {

  int i, j;

  Symbol** grid = (Symbol**)malloc(gridsize * sizeof(Symbol*));
  for (i = 0; i < gridsize; i++) {
    grid[i] = malloc(gridsize * sizeof(Symbol));
  }

  for (i = 0; i < gridsize; i++) {
    for (j = 0; j < gridsize; j++) {
      strcpy(grid[i][j], "0");
    }
  }

  return grid;

}
void freeGrid (Symbol** grid, int gridsize) {

    int i;
    for (i = 0; i < gridsize; i++) {
        free(grid[i]);
    }
    free(grid);

}


void* clientThread (void* argument) {

    ClientThreadArg* arg = (ClientThreadArg*)argument;
    arg->thread = pthread_self();
    snprintf(arg->nickname, MAX_CREDENTIALS_LENGTH, "%d", arg->socket);  // Il nickname di default è la socket

    int thread_socket = arg->socket;

    char client_buffer[BUFFER_LENGTH];

    int client_action;
    int signupResult, loginResult, sendlistResult;


    while (1) {

        memset(client_buffer, 0, strlen(client_buffer));
        if (read(thread_socket, client_buffer, BUFFER_LENGTH) <= 0) {
            onClientDisconnection(arg);
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
                    if (send(thread_socket, SIGNUP_SUCCESS, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        printf("Il client %d registra un utente\n\n", thread_socket);
                    }
                }
                else {
                    if (send(thread_socket, USER_ALREADY_EXISTS, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
            break;

            case LOGIN:
                pthread_mutex_lock(&login_lock);
                loginResult = logIn(client_buffer, arg);
                pthread_mutex_unlock(&login_lock);
                if (loginResult == 0) {
                    if (send(thread_socket, LOGIN_SUCCESS, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        pthread_mutex_lock(&activeUsers_lock);
                        activeUsersList = push(activeUsersList, arg->nickname);
                        pthread_mutex_unlock(&activeUsers_lock);
                        printf("%s si connette sul client %d\n\n", arg->nickname, thread_socket);
                    }
                }
                else if (loginResult == 1) {
                    memset(arg->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(thread_socket, USER_DOESNT_EXIST, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
                else {
                    memset(arg->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(thread_socket, WRONG_PASSWORD, 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
            break;

            case VIEW_ACTIVE_USERS:
                pthread_mutex_lock(&activeUsers_lock);
                sendlistResult = sendUsersList(arg);
                pthread_mutex_unlock(&activeUsers_lock);
                if (sendlistResult == FALSE) {
                    onClientDisconnection(arg);
                    close(thread_socket);
                    pthread_exit(NULL);
                }
            break;

            case LOOK_FOR_MATCH:
                pthread_mutex_lock(&lookingForMatch_lock);
                usersLookingForMatch = append(usersLookingForMatch, arg);
                pthread_mutex_unlock(&lookingForMatch_lock);
                if (length(usersLookingForMatch) == NUMBER_OF_PLAYERS) {
                    pthread_kill(main_thread, SIGUSR1);
                }
                pthread_mutex_lock(&condition_mutex[arg->serial]);
                printf("Client %d bloccato\n", thread_socket);
                while (!can_be_woken[arg->serial]) {
                    pthread_cond_wait(&wake_conditions[arg->serial], &condition_mutex[arg->serial]);
                }
                can_be_woken[arg->serial] = FALSE;
                pthread_mutex_unlock(&condition_mutex[arg->serial]);
                printf("Client %d ripreso\n", thread_socket);
            break;

            case LOGOUT:
                printf("%s effettua il logout dal client %d\n", arg->nickname, arg->socket);
                pthread_mutex_lock(&activeUsers_lock);
                activeUsersList = delete(activeUsersList, arg->nickname, areEqual_str);
                pthread_mutex_unlock(&activeUsers_lock);
                memset(arg->nickname, 0, MAX_CREDENTIALS_LENGTH);
            break;

            case STOP_LOOKING_FOR_MATCH:

            break;

            default:
                send(thread_socket, "DEFAULT\n", strlen("DEFAULT\n"), MSG_NOSIGNAL);   // da togliere
            break;

        }

    }

}


void* gameThread (void* argument) {

    printf("game thread started\n\n");

    GameThreadArg* arg = (GameThreadArg*)argument;

    int gridsize = 7, win = 2;       // Le dimensioni e il win devono andare in accordo col numero di giocatori (saranno #define)
    Symbol** grid = makeGrid(gridsize);
    int winIsReached = FALSE;
    Player* players[NUMBER_OF_PLAYERS];
    int valsend, valread;
    int x, y;
    int new_x, new_y;
    char x_str[3], y_str[3], new_x_str[3], new_y_str[3];
    int active_player = 0, defending_player, atk, def;
    char atk_str[2] = "", def_str[2] = "";
    char active_player_str[3] = "", defending_player_str[3] = "";
    char ack[2] = "";
    char move[2] = "";
    char outcome[2] = "";
    char server_msg[20] = "";
    char player_data[50] = "";

    memcpy(players, arg->players, sizeof(players));

    int i, j;
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {                                             // INVIO MESSAGGIO DI INIZIO PARTITA
        if ((valsend = send(players[i]->socket, "1\n", 2, MSG_NOSIGNAL)) < 0) {
            printf("Errore di ricezione dal client %d\n", players[i]->socket);
            pthread_exit(NULL);
        } printf("Sent 1\n");
        if ((valread = read(players[i]->socket, ack, 2)) < 0) {
            printf("Errore di ricezione dal client %d\n", players[i]->socket);
            pthread_exit(NULL);
        } printf("Read %s\n", ack);
        memset(ack, 0, 2);
    }
    printf("Clients notified\n");

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        strcat(player_data, players[i]->nickname);
        strcat(player_data, "|");
        snprintf(players[i]->symbol, SYMBOL_SIZE, "%d", i+1);
        strcat(player_data, players[i]->symbol);
        strcat(player_data, "|");
        players[i]->territories = 1;
        players[i]->isActive = FALSE;
        do {
            x = rand()%gridsize;
            y = rand()%gridsize;
        } while (!(strcmp(grid[x][y], "0") == 0));
        players[i]->x = x;
        players[i]->y = y;
        sprintf(x_str, "%d", x);
        sprintf(y_str, "%d", y);
        strcat(player_data, x_str);
        strcat(player_data, "|");
        strcat(player_data, y_str);
        strcat(player_data, "\n");
        strcpy(grid[x][y], players[i]->symbol);
        for (j = 0; j < NUMBER_OF_PLAYERS; j++) {                                                                   // INVIO DEI DATI DEI GIOCATORI
            if ((valsend = send(players[j]->socket, player_data, strlen(player_data), MSG_NOSIGNAL)) < 0) {
                printf("Errore di ricezione dal client %d\n", players[j]->socket);
                pthread_exit(NULL);
            } printf("Sent %d bytes\n", valsend);
            if ((valread = read(players[j]->socket, ack, 2)) < 0) {
                printf("Errore di ricezione dal client %d\n", players[j]->socket);
                pthread_exit(NULL);
            } printf("Read %s", ack);
            memset(ack, 0, 2);
        }
        memset(player_data, 0, 50);
    }

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {                                              // INVIO TERMINAZIONE DEI DATI DEI GIOCATORI
        if ((valsend = send(players[i]->socket, "|\n", 2, MSG_NOSIGNAL)) < 0) {
            printf("Errore di ricezione dal client %d\n", players[i]->socket);
            pthread_exit(NULL);
        } printf("Sent termination char\n");
        if ((valread = read(players[i]->socket, ack, 2)) < 0) {
            printf("Errore di ricezione dal client %d\n", players[i]->socket);
            pthread_exit(NULL);
        } printf("Read %s\n", ack);
        memset(ack, 0, 2);
    }

    printf("Player data sent\n");
    while (!winIsReached) {

        if (active_player == NUMBER_OF_PLAYERS) {
            active_player = 0;
        }

        sprintf(active_player_str, "%d", active_player);

        strcat(server_msg, "1");                  // 1 per segnalare al client che sta venendo inviato il giocatore attivo
        strcat(server_msg, active_player_str);
        strcat(server_msg, "\n");

        for (i = 0; i < NUMBER_OF_PLAYERS; i++) {                                                         // INVIO DEL GIOCATORE ATTIVO
            if ((valsend = send(players[i]->socket, server_msg, strlen(server_msg), MSG_NOSIGNAL)) < 0) {
                printf("Errore di ricezione dal client %d\n", players[i]->socket);
                pthread_exit(NULL);
            } printf("Sent active player position\n");
            if ((valread = read(players[i]->socket, ack, 2)) < 0) {
                printf("Errore di ricezione dal client %d\n", players[i]->socket);
                pthread_exit(NULL);
            } printf("Read %s\n", ack);
            memset(ack, 0, 2);
        }
        memset(server_msg, 0, sizeof(server_msg));

        printf("Il giocatore attivo %s e' in posizione (%d, %d)\n", players[active_player]->nickname, players[active_player]->x, players[active_player]->y);

        memset(move, 0, strlen(move));
        if (read(players[active_player]->socket, move, 2) <= 0) {                                 // LETTURA DELLA MOSSA
            printf("Errore di ricezione dal client %d\n", players[active_player]->socket);
            pthread_exit(NULL);
        } printf("Mossa letta: %s\n", move);

        move[1] = '\0';

        switch (move[0]) {

            case 'N':
                new_x = players[active_player]->x;
                new_y = players[active_player]->y - 1;
            break;

            case 'S':
                new_x = players[active_player]->x;
                new_y = players[active_player]->y + 1;
            break;

            case 'O':
                new_x = players[active_player]->x - 1;
                new_y = players[active_player]->y;
            break;

            case 'E':
                new_x = players[active_player]->x + 1;
                new_y = players[active_player]->y;
            break;

            default:
                printf("Mossa non valida, errore di ricezione dal client %d\n", players[active_player]->socket);
                pthread_exit(NULL);
            break;

        }

        printf("%s vuole spostarsi in posizione (%d, %d)\n", players[active_player]->nickname, new_x, new_y);

        if (strcmp(grid[new_x][new_y], "0") != 0 && strcmp(grid[new_x][new_y], players[active_player]->symbol) != 0) {     // CASELLA OCCUPATA
            defending_player = atoi(grid[new_x][new_y]) - 1;
            sprintf(defending_player_str, "%d", defending_player);
            printf("La posizione (%d, %d) è occupata dal giocatore %s\n", new_x, new_y, players[defending_player]->nickname);
            atk = rand()%6 + 1;
            def = rand()%6 + 1;
            sprintf(atk_str, "%d", atk);
            sprintf(def_str, "%d", def);
            printf("ATK: %d DEF: %d\n", atk, def);
            if (atk > def) {                                                                           // VITTORIA ATTACCO
                strcpy(grid[new_x][new_y], players[active_player]->symbol);
                players[active_player]->territories++;
                players[defending_player]->territories--;
                players[active_player]->x = new_x;
                players[active_player]->y = new_y;
                sprintf(outcome, "%d", 2);
                printf("Vince l'attacco: %s si sposta in posizione (%d, %d). %s possiede ora %d territori e il giocatore %s ne possiede %d\n\n", players[active_player]->nickname, players[active_player]->x, players[active_player]->y, players[active_player]->nickname, players[active_player]->territories, players[defending_player]->nickname, players[defending_player]->territories);
            }
            else {                                                                                  // VITTORIA DIFESA
                sprintf(outcome, "%d", 3);
                printf("Vince la difesa del giocatore %s, spostamento non effettuato\n\n", players[defending_player]->nickname);
            }
        }
        else {                  // CASELLA LIBERA O GIA' POSSEDUTA
            if (strcmp(grid[new_x][new_y], players[active_player]->symbol) == 0) {        // SPOSTAMENTO IN CELLA GIA' POSSEDUTA
                sprintf(outcome, "%d", 4);
                players[active_player]->x = new_x;
                players[active_player]->y = new_y;
                printf("Posizione gia' posseduta: spostamento effettuato.\n");
            }
            else {                                           // SPOSTAMENTO IN CELLA LIBERA
                sprintf(outcome, "%d", 5);
                strcpy(grid[new_x][new_y], players[active_player]->symbol);
                players[active_player]->territories++;
                players[active_player]->x = new_x;
                players[active_player]->y = new_y;
                printf("Posizione libera: spostamento effettuato. %s ora possiede %d territori\n\n", players[active_player]->nickname, players[active_player]->territories);
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

        for (i = 0; i < NUMBER_OF_PLAYERS; i++) {                                                                // INVIO ESITO MOSSA
            if ((valsend = send(players[i]->socket, server_msg, strlen(server_msg), MSG_NOSIGNAL)) < 0) {
                printf("Errore di ricezione dal client %d durante l'invio esito\n", players[i]->socket);
                pthread_exit(NULL);
            } printf("Esito mossa inviato: %d byte\n", valsend);
            if ((valread = read(players[i]->socket, ack, 2)) < 0) {
                printf("Errore di ricezione dal client %d durante l'invio esito\n", players[i]->socket);
                pthread_exit(NULL);
            } printf("Risposta del client: %s\n", ack);
            memset(ack, 0, 2);
        }
        memset(server_msg, 0, sizeof(server_msg));

        if (players[active_player]->territories == win) {                                                      // INVIO VITTORIA
            printf("%s vince la partita\n\n", players[active_player]->nickname);
            winIsReached = TRUE;
            for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
                pthread_mutex_lock(&condition_mutex[players[i]->serial]);
                can_be_woken[players[i]->serial] = TRUE;
                pthread_cond_signal(&wake_conditions[players[i]->serial]);
                pthread_mutex_unlock(&condition_mutex[players[i]->serial]);
            }
        }

        active_player++;

    }

    freeGrid(grid, gridsize);
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        free(players[i]);
    }

}


void start_match (int signo) {

    printf("start_match invoked\n");
    static int matchnum = 0;

    GameThreadArg* arg = (GameThreadArg*)malloc(sizeof(GameThreadArg));
    ClientThreadArg* player_data;
    Player* player = NULL;

    pthread_mutex_lock(&lookingForMatch_lock);
    List* l = usersLookingForMatch;
    pthread_mutex_unlock(&lookingForMatch_lock);

    int i = 0;
    for (; l; l = l->next) {
        player_data = (ClientThreadArg*)(l->data);
        player = (Player*)malloc(sizeof(Player));
        strcpy(player->nickname, player_data->nickname);
        player->socket = player_data->socket;
        player->thread = player_data->thread;
        player->serial = player_data->serial;
        arg->players[i] = player;
        i++;
    }

    pthread_mutex_lock(&lookingForMatch_lock);
    usersLookingForMatch = freelist(usersLookingForMatch, NULL);
    pthread_mutex_unlock(&lookingForMatch_lock);

    pthread_create(&(matchThread_id[matchnum++]), NULL, &gameThread, arg);   // Bisogna creare un mutex e usarlo qui per matchThread_id

}



int main (int argc, char* argv[]) {

    signal(SIGUSR1, start_match);

    srand(time(NULL));

    main_thread = pthread_self();

    pthread_t clientThread_id[40];
    int clientnum = 0;

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int options = 1;

    int listener_socket_fd = 0, new_socket = 0;

    ClientThreadArg* arg;



    int i;
    for (i = 0; i < 40; i++) {
        pthread_cond_init(&wake_conditions[i], NULL);
        pthread_mutex_init(&condition_mutex[i], NULL);
        can_be_woken[i] = FALSE;
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
            arg = (ClientThreadArg*)malloc(sizeof(ClientThreadArg));
            arg->socket = new_socket;
            arg->serial = clientnum;
            pthread_create(&(clientThread_id[clientnum++]), NULL, &clientThread, arg);
        }

    }

    exit(0);
}
