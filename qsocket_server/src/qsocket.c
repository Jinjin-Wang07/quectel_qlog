#include "qsocket.h"
#include "common.h"

TCPServer *create_server(int port)
{
    TCPServer *server = (TCPServer *)malloc(sizeof(TCPServer));
    if (!server)
    {
        perror("Failed to allocate server");
        return NULL;
    }

    server->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket_fd < 0)
    {
        perror("Socket creation failed");
        free(server);
        return NULL;
    }

    memset(&server->server_addr, 0, sizeof(server->server_addr));
    server->server_addr.sin_family = AF_INET;
    server->server_addr.sin_addr.s_addr = INADDR_ANY;
    server->server_addr.sin_port = htons(port);

    snprintf(server->filename, sizeof(server->filename), "server_output_%d.txt", port);
    server->file = fopen(server->filename, "wb");
    server->current_size = 0;

    return server;
}

int start_server(TCPServer *server)
{
    if (bind(server->socket_fd, (struct sockaddr *)&server->server_addr, sizeof(server->server_addr)) < 0)
    {
        perror("Bind failed");
        return -1;
    }

    if (listen(server->socket_fd, MAX_CONNECTIONS) < 0)
    {
        perror("Listen failed");
        return -1;
    }

    printf("Server is listening on port %d\n", ntohs(server->server_addr.sin_port));
    return 0;
}

void rotate_file(TCPServer *server) { return; }

void handle_client(TCPServer *server)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;

    while ((bytes_received = recv(server->socket_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        fwrite(buffer, 1, bytes_received, server->file);
        server->current_size += bytes_received;

        if (server->current_size >= MAX_FILE_SIZE)
        {
            rotate_file(server);
        }
    }

    if (bytes_received < 0)
    {
        perror("Receive failed");
    }

    fclose(server->file);
}

void cleanup_server(TCPServer *server)
{
    close(server->socket_fd);
    if (server->file)
    {
        fclose(server->file);
    }
    free(server);
}
