#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct
{
    int socket_fd;
    struct sockaddr_in server_addr;
    FILE *file;
    char filename[256];
    long current_size;
} TCPServer;

TCPServer *create_server(int port);
int start_server(TCPServer *server);
void handle_client(TCPServer *server);
void cleanup_server(TCPServer *server);

#endif // TCP_SERVER_H
