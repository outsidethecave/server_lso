#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct List {
    void* data;
    struct List* next;
} List;


int areEqual (void* p1, void* p2);

void printNode (List* node);


List* newNode (void* data);

List* append (List* list, void* data);

List* delete (List* list, void* data, int (*areEqual)(void*, void*));

void freelist (List* list);

void print (List* list, void (*printNode)(List*));

#endif
