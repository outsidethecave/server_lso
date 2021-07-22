#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct List {
    void* data;
    struct List* next;
} List;


int areEqual_str (void* p1, void* p2);

void printNode_str (List* node);

char* toString_str (void* p1);


List* newNode (void* data);

List* push (List* list, void* data);

List* append (List* list, void* data);

List* delete (List* list, void* data, int (*areEqual)(void*, void*));

int length (List* list);

void freelist (List* list);

void print (List* list, void (*printNode)(List*));

#endif
