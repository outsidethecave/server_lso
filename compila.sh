#!/bin/bash

gcc -pthread -o server server.c llist.c definizioni.h
./server
