#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"


int setup() {
    /** Server Configuration */
    int server_fd = -1;
    int rcode = -1;
    int reuse_port = 1;
    struct addrinfo hints = { 0 };
    struct addrinfo *server_opts = NULL;
    struct addrinfo *server_info = NULL;

    /** Server Setup */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rcode = getaddrinfo(NULL, PORT, &hints, &server_opts)) != 0)
    {
        printf("getaddrinfo error: %s\n", gai_strerror(rcode));
        return -1;
    }

    for (server_info = server_opts; server_info; server_info = server_info->ai_next)
    {
        if ((server_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol)) == -1)
        {
            printf("socket error: %s\n", strerror(errno));
            continue;
        }
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(int)) == -1)
        {
            printf("setsockopt error %s:\n", strerror(errno));

            freeaddrinfo(server_opts);

            return -1;
        }
        if (bind(server_fd, server_info->ai_addr, server_info->ai_addrlen) == -1)
        {
            close(server_fd);
            printf("bind error %s:\n", strerror(errno));
            continue;
        }
        break;
    }

    freeaddrinfo(server_opts);

    if (!server_info)
    {
        printf("Failed to bind\n");
        return -1;
    }

    if (listen(server_fd, BACKLOG) == -1)
    {
        printf("listen error: %s\n", strerror(errno));
        return -1;
    }

    return server_fd;
}
