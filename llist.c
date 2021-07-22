#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llist.h"

int areEqual_str (void* p1, void* p2) {
    char* arg1 = (char*)p1;
    char* arg2 = (char*)p2;

    return (strcmp(arg1, arg2) == 0);
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
    return 0;
}

void freelist (List* list) {
    if (list) {
        freelist(list->next);
        free(list->data);
        free(list);
    }
}

void print (List* list, void (*printNode)(List*)) {
    if (list) {
        printNode(list);
        print(list->next, printNode);
    }
}
