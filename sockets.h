#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888

int create_client_socket();

int create_socket();
int connect_client(int);
