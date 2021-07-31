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

#define TRUE 1
#define FALSE 0

#define MAX_CREDENTIALS_LENGTH 50


typedef struct List {
    void* data;
    struct List* next;
} List;

typedef struct ClientThreadArg {
    char nickname[MAX_CREDENTIALS_LENGTH];
    int socket;
    pthread_t thread;
    int pipe[2];
} ClientThreadArg;

void freeData_baseType (void* p) {
    free(p);
}

int areEqual_str (void* p1, void* p2) {
    char* arg1 = (char*)p1;
    char* arg2 = (char*)p2;

    return (strcmp(arg1, arg2) == 0);
}

int areEqual_int (void* p1, void* p2) {
    int arg1 = *((int*)p1);
    int arg2 = *((int*)p2);

    return arg1 == arg2;
}

void printNode_str (List* node) {
    char* data = (char*)node->data;
    if (node->next) {
        printf("%s -> ", data);
    }
    else {
        printf("%s -> NULL\n", data);
    }
}

void printNode_int (List* node) {

    if (!node) return;

    int data = *((int*)(node->data));
    if (node->next) {
        printf("%d -> ", data);
    }
    else {
        printf("%d -> NULL\n", data);
    }

}

char* toString_str (void* p1) {
    return ((char*)p1);
}



List* newNode (void* data) {
    List* newNode = (List*)malloc(sizeof(List));
    if (newNode) {
        newNode->data = data;
        newNode->next = NULL;
    }
    return newNode;
}

List* push (List* list, void* data) {
    List* tmp = newNode(data);
    tmp->next = list;
    return tmp;
}

List* append (List* list, void* data) {
    if (list) {
        list->next = append(list->next, data);
    }
    else {
        list = newNode(data);
    }
    return list;
}

List* delete (List* list, void* data, int (*areEqual)(void*, void*)) {
    if (list) {
        if (areEqual(list->data, data)) {
            List* tmp = list->next;
            free(list);
            return tmp;
        }
        else if (list->next && areEqual(list->next->data, data)) {
            List* tmp = list->next;
            list->next = tmp->next;
            free(tmp);
            return list;
        }
        else {
            list->next = delete(list->next, data, areEqual);
            return list;
        }
    }
    return NULL;
}

int length (List* list) {
    if (list) {
        return 1 + length(list->next);
    }
    else {
        return 0;
    }
}

List* freelist (List* list, void (*freeData)(void*)) {

    List* tmp;

    while (list) {
        tmp = list;
        list = list->next;
        if (freeData) {
            freeData(tmp->data);
        }
        free(tmp);
    }

    return NULL;

}

void print (List* list, void (*printNode)(List*)) {
    if (list) {
        printNode(list);
        print(list->next, printNode);
    }
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

void popola (char array[]) {
    int i = 0;
    for (; i < 50; i++) {
        array[i] = 'A';
    }
    array[i] = '\0';
}

ClientThreadArg* global;

pthread_t prendiself() {
    return pthread_self();
}

void* threadfun (void* arg) {
    if (pthread_equal(pthread_self(), prendiself())) {
        printf("Uguali\n");
    }
    else {
        printf("Diversi\n");
    }
}


void concatena (char* str1, char* str2) {
    strcat(str1, str2);
}

int makeTimestamp(char * tsBuff) {
  time_t curSec;
  struct tm * curDate;
  char dateString[32];

  curSec = time(NULL);
  curDate = localtime( & curSec);
  sprintf(tsBuff, "%02d-%02d-%d %02d:%02d:%02d",
    curDate -> tm_mday,
    curDate -> tm_mon + 1,
    curDate -> tm_year + 1900,
    curDate -> tm_hour,
    curDate -> tm_min,
    curDate -> tm_sec);

  return 0;
}

int main() {

    char* str = "provaprova";
    printf("%d\n", (int)strlen(str));

}
