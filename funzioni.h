#ifndef FUNZIONI_H
#define FUNZIONI_H

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


//[START] Funzioni di logging

void makeTimestamp(char* timestamp);

void log_ClientConnection (int clientSocket, struct in_addr ip);
void log_SignUp (char* nickname, int clientSocket);
void log_SignIn (char* nickname, int clientSocket);
void log_SignOut (char* nickname, int clientSocket);
void log_GameStart (Game* game);
void log_GameConquest (Game* game, int defendingPlayerIndex);
void log_GameEnd (Game* game, char* winner);
void log_ClientDisconnection (int clientSocket);

//[END] Funzioni di logging


//[START] Funzioni del client

int saveCredentialsToFile (char* credentialsBuffer, int socket);
int userExists (char* nickname);
int signIn (char* credentialsBuffer, Client* client);
int isConnected (char* nickname);
void onClientDisconnection (Client* client);

void sendUsersList (Client* client);

Client* createClient (ulong id, int socket);
void* clientThread (void* client);
void handleSignUpResult (Client* client, int signUpResult);
void handleSignInResult (Client* client, int signInResult);
void handleLeaveMatch (Client* client);

int areEqual_cli (void* p1, void* p2);
int areEqual_str (void* p1, void* p2);

//[END] Funzioni del client


//[START] Funzioni di gioco

Symbol** makeGrid ();
void freeGrid (Symbol** grid);

void start_match ();
Game* createGame ();
void makePlayers (Game* game);
void notifyPlayersOfStartMatch (Game* game);
void initGame (Game* game);
void sendDataToAllPlayers (Game* game, char* data);

int areEqual_game (void* p1, void* p2);
Game* getGameByID (List* list, ulong id);

void* gameThread (void* game);
void setNewActivePlayer (Game* game, int* activePlayerIndex);
void sendActivePlayerAndTimerEnd (Game* game, int activePlayerIndex);
void* timerThread (void* argument);
void handleMoveTimeout (Game* game);
void handleMatchLeftFromActivePlayer (Game* game, int activePlayerIndex);
int squareIsOwnedByEnemy (Game* game, int x, int y);
void handleSquareIsOwnedByEnemy (Game* game, int x, int y, char moveToSend);
int squareIsOwnedBySelf (Game* game, int x, int y);
void handleSquareIsOwnedBySelf (Game* game, int x, int y, char moveToSend);
void handleSquareIsFree (Game* game, int x, int y, char moveToSend);
void endGame (Game* game, char* winner);

//[END] Funzioni di gioco

void setGridSizeAndWinCondition ();
void prepareRand();

#endif
