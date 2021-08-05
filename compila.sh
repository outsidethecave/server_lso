#!/bin/bash

gcc -pthread -o server server.c llist.c funzioni.c 
./server $1
