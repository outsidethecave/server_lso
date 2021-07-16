#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llist.h"

int areEqual (void* p1, void* p2) {
    char* arg1 = (char*)p1;
    char* arg2 = (char*)p2;
    return strcmp(arg1, arg2) == 0;
}

void printNode (List* node) {
    char* data = (char*)node->data;
    if (node->next) {
        printf("%s -> ", data);
    }
    else {
        printf("%s -> NULL\n", data);
    }
}


List* newNode (void* data) {
    List* newNode = (List*)malloc(sizeof(List));
    if (newNode) {
        memcpy(newNode->data, data, sizeof(data));
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

void freelist (List* list) {
    if (list) {
        freelist(list->next);
        free(list);
    }
}

void print (List* list, void (*printNode)(List*)) {
    if (list) {
        printNode(list);
        print(list->next, printNode);
    }
}
