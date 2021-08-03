#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llist.h"
#include "definizioni.h"



int areEqual_str (void* p1, void* p2) {
    char* arg1 = (char*)p1;
    char* arg2 = (char*)p2;

    return (strcmp(arg1, arg2) == 0);
}

int areEqual_cli (void* p1, void* p2) {
    Client* arg1 = (Client*)p1;
    Client* arg2 = (Client*)p2;

    return strcmp(arg1->nickname, arg2->nickname) == 0;
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



Game* getGameByID (List* list, int id) {

    if (list) {

        if (((Game*)list->data)->id == id) {
            return list->data;
        }
        else {
            return getGameByID(list->next, id);
        }

    }

    return NULL;

}

List* deleteGameByID (List* list, int id) {
    if (list) {
        if (((Game*)list->data)->id == id) {
            List* tmp = list->next;
            free(list);
            return tmp;
        }
        else if (list->next && ((Game*)list->next->data)->id == id) {
            List* tmp = list->next;
            list->next = tmp->next;
            free(tmp);
            return list;
        }
        else {
            list->next = deleteGameByID(list->next, id);
            return list;
        }
    }
    return NULL;
}

List* deleteClientByID (List* list, int id) {
    if (list) {
        if (((Client*)list->data)->id == id) {
            List* tmp = list->next;
            free(list);
            return tmp;
        }
        else if (list->next && ((Client*)list->next->data)->id == id) {
            List* tmp = list->next;
            list->next = tmp->next;
            free(tmp);
            return list;
        }
        else {
            list->next = deleteClientByID(list->next, id);
            return list;
        }
    }
    return NULL;
}
