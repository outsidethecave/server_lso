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
#define NUMBER_OF_PLAYERS 2
#define SIGNUP 1
#define LOGIN 2
#define VIEW_ACTIVE_USERS 3
#define FIND_MATCH 4


typedef char Symbol[SYMBOL_SIZE];

typedef struct ClientThreadArg {
    char nickname[MAX_CREDENTIALS_LENGTH];
    int socket;
    pthread_t thread;
} ClientThreadArg;

typedef struct Player {
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
pthread_t matchThread_id[10];
int users_fd;
List* activeUsersList = NULL;
List* usersLookingForMatch = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
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
    snprintf(arg->nickname, 256, "%d", arg->socket);

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
                    if (send(thread_socket, "1\n", 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                    else {
                        printf("Il client %d registra un utente\n\n", thread_socket);
                    }
                }
                else {
                    if (send(thread_socket, "0\n", 2, MSG_NOSIGNAL) < 0) {
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
                    if (send(thread_socket, "0\n", 2, MSG_NOSIGNAL) < 0) {
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
                    if (send(thread_socket, "1\n", 2, MSG_NOSIGNAL) < 0) {
                        onClientDisconnection(arg);
                        close(thread_socket);
                        pthread_exit(NULL);
                    }
                }
                else {
                    memset(arg->nickname, 0, MAX_CREDENTIALS_LENGTH);
                    if (send(thread_socket, "2\n", 2, MSG_NOSIGNAL) < 0) {
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

            case FIND_MATCH:
                pthread_mutex_lock(&lookingForMatch_lock);
                usersLookingForMatch = append(usersLookingForMatch, arg);
                pthread_mutex_unlock(&lookingForMatch_lock);
                if (length(usersLookingForMatch) == NUMBER_OF_PLAYERS) {
                    pthread_kill(main_thread, SIGUSR1);
                }
                else {

                }
            break;

            default:
                send(thread_socket, "DEFAULT\n", strlen("DEFAULT\n"), MSG_NOSIGNAL);   // da togliere
            break;

        }

    }

}


void* gameThread (void* argument) {

    printf("gameThread started (id: %lu)\n\n", pthread_self());

    GameThreadArg* arg = (GameThreadArg*)argument;

    int gridsize = 10, win = 10;       // Le dimensioni e il win devono andare in accordo col numero di giocatori
    Symbol** grid = makeGrid(gridsize);
    int winIsReached = FALSE;
    Player* players[NUMBER_OF_PLAYERS];
    int valsend;
    int x, y;
    int new_x, new_y;
    char x_str[3], y_str[3], new_x_str[3], new_y_str[3];
    int active_player = 0, defending_player, atk, def;
    char move[2];
    char player_data[50] = "";

    memcpy(players, arg->players, sizeof(players));

    int i, j;
    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (send(players[i]->socket, "1\n", 2, MSG_NOSIGNAL) < 0) {
            printf("Errore di ricezione dal client %d\n", players[i]->socket);
            pthread_exit(NULL);
        }
    }

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
        snprintf(x_str, 2, "%d", x);
        snprintf(y_str, 2, "%d", y);
        strcat(player_data, x_str);
        strcat(player_data, "|");
        strcat(player_data, y_str);
        strcat(player_data, "\n");
        strcpy(grid[x][y], players[i]->symbol);
        for (j = 0; j < NUMBER_OF_PLAYERS; j++) {
            printf("Data: %s", player_data);
            if ((valsend = send(players[j]->socket, player_data, strlen(player_data), MSG_NOSIGNAL)) < 0) {
                printf("Errore di ricezione dal client %d\n", players[j]->socket);
                pthread_exit(NULL);
            }
            printf("Bytes sent: %d\n", valsend);
        }
        memset(player_data, 0, 50);
    }

    for (i = 0; i < NUMBER_OF_PLAYERS; i++) {
        if (send(players[i]->socket, "|\n", 2, MSG_NOSIGNAL) < 0) {
            printf("Errore di ricezione dal client %d\n", players[i]->socket);
            pthread_exit(NULL);
        }
    }

    while (!winIsReached) {

        if (active_player == NUMBER_OF_PLAYERS) {
            active_player = 0;
        }

        printf("Il giocatore attivo %s e' in posizione (%d, %d)\n", players[active_player]->nickname, players[active_player]->x, players[active_player]->y);

        if (read(players[active_player]->socket, move, 2) <= 0) {
            printf("Errore di ricezione dal client %d\n", players[active_player]->socket);
            pthread_exit(NULL);
        }

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

        if (strcmp(grid[new_x][new_y], "0") != 0) {
            defending_player = atoi(grid[new_x][new_y]) - 1;
            printf("La posizione (%d, %d) è occupata dal giocatore %s\n", new_x, new_y, players[defending_player]->nickname);
            atk = rand()%6 + 1;
            def = rand()%6 + 1;
            printf("ATK: %d DEF: %d\n", atk, def);
            if (atk > def) {
                strcpy(grid[new_x][new_y], players[active_player]->symbol);
                players[active_player]->territories++;
                players[defending_player]->territories--;
                players[active_player]->x = new_x;
                players[active_player]->y = new_y;
                printf("Vince l'attacco: %s si sposta in posizione (%d, %d). %s possiede ora %d territori e il giocatore %s ne possiede %d\n\n", players[active_player]->nickname, players[active_player]->x, players[active_player]->y, players[active_player]->nickname, players[active_player]->territories, players[defending_player]->nickname, players[defending_player]->territories);
                if (send(players[active_player]->socket, "1\n", 2, MSG_NOSIGNAL) < 0) {
                    printf("Errore di ricezione dal client %d\n", players[i]->socket);
                    pthread_exit(NULL);
                }
            }
            else {
                printf("Vince la difesa del giocatore %s, spostamento non effettuato\n\n", players[defending_player]->nickname);
                if (send(players[active_player]->socket, "0\n", 2, MSG_NOSIGNAL) < 0) {
                    printf("Errore di ricezione dal client %d\n", players[i]->socket);
                    pthread_exit(NULL);
                }
            }
        }
        else {
            strcpy(grid[new_x][new_y], players[active_player]->symbol);
            players[active_player]->territories++;
            players[active_player]->x = new_x;
            players[active_player]->y = new_y;
            printf("Posizione libera: spostamento effettuato. %s ora possiede %d territori\n\n", players[active_player]->nickname, players[active_player]->territories);
            if (send(players[active_player]->socket, "1\n", 2, MSG_NOSIGNAL) < 0) {
                printf("Errore di ricezione dal client %d\n", players[i]->socket);
                pthread_exit(NULL);
            }
        }

        if (players[active_player]->territories == win) {
            printf("%s vince la partita\n\n", players[active_player]->nickname);
            winIsReached = TRUE;
            if (send(players[active_player]->socket, "2\n", 2, MSG_NOSIGNAL) < 0) {
                printf("Errore di ricezione dal client %d\n", players[i]->socket);
                pthread_exit(NULL);
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
        arg->players[i] = player;
        printf("Player %d: %s %d %lu\n", i, arg->players[i]->nickname, arg->players[i]->socket, arg->players[i]->thread);
        i++;
    }

    pthread_mutex_lock(&lookingForMatch_lock);
    freelist(usersLookingForMatch);
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
            pthread_create(&(clientThread_id[clientnum++]), NULL, &clientThread, arg);
        }

    }

    exit(0);
}
