#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

typedef struct List {
    void* data;
    struct List* next;
} List;


int areEqual (void* p1, void* p2) {
    int arg1 = *((int*)p1);
    int arg2 = *((int*)p2);
    return arg1 == arg2;
}

void printNode (List* node) {
    int data = *((int*)node->data);
    if (node->next) {
        printf("%d -> ", data);
    }
    else {
        printf("%d -> NULL\n", data);
    }
}


List* newNode (void* data) {
    List* newNode = (List*)malloc(sizeof(List));
    if (newNode) {
        newNode->data = data;
        newNode->next = NULL;
    }
    return newNode;
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

void print (List* list, void (*printNode)(List*)) {
    if (list) {
        printNode(list);
        print(list->next, printNode);
    }
}

int main () {

    List* L = NULL;

    int elems[] = {1, 7, -3, 4, 12, 0};

    int i = 0;
    for (; i < 6; i++) {
        L = append(L, &elems[i]);
    }

    print(L, printNode);

    L = delete (L, &elems[0], areEqual);
    L = delete (L, &elems[0], areEqual);

    print(L, printNode);

}
