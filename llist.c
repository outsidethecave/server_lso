#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llist.h"
#include "definizioni.h"



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

int contains (List* list, void* data, int (*areEqual)(void*, void*)) {

    if (list) {
        if (areEqual(list->data, data)) {
            return TRUE;
        }
        else {
            return contains(list->next, data, areEqual);
        }
    }

    return FALSE;

}

List* delete (List* list, void* data, int (*areEqual)(void*, void*), void (*freeData)(void*)) {
    if (list) {
        if (areEqual(list->data, data)) {
            List* tmp = list->next;
            if (freeData) {
                freeData(list->data);
            }
            free(list);
            return tmp;
        }
        else if (list->next && areEqual(list->next->data, data)) {
            List* tmp = list->next;
            list->next = tmp->next;
            if (freeData) {
                freeData(tmp->data);
            }
            free(tmp);
            return list;
        }
        else {
            list->next = delete(list->next, data, areEqual, freeData);
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
