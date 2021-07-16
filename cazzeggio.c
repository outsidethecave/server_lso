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

void popola (char array[]) {
    int i = 0;
    for (; i < 50; i++) {
        array[i] = 'A';
    }
    array[i] = '\0';
}

int main() {

    char string[50];
    popola(string);
    printf("%s\n", string);

}
