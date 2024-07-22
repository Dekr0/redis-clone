#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "event_loop.h"
#include "ip.h"
#include "thread_pool.h"

#define RECV_BUF_SIZE 256

#define DEBUG

struct message_t {
    char buffer[256];
    char addrress[INET6_ADDRSTRLEN];
};

int32_t thread_pool_server(int32_t);

void handle_client(int, int);

int32_t event_loop_server(int32_t);

void client_socket_handle(struct event_loop_t*, int, void *);

void server_socket_handle(struct event_loop_t*, int, void *);

int main() {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
    int server_fd = -1;
    if ((server_fd = setup()) == -1) {
        return 1;
    }

    return event_loop_server(server_fd);
    // return thread_pool_server(server_fd);
}

int event_loop_server(int32_t server_fd) {
    struct event_loop_t* el = create_event_loop(DEFAULT_FDS_CAP);
    if (el == NULL) {
        return 1;
    }
    if (register_event(el, server_fd, E_READABLE, server_socket_handle, NULL) 
            == ERANGE) {
        return 1;
    }

    int rval = POLL_ERR;
    while (!el->stop) {
        if ((rval = process_events(el)) == POLL_ERR) {
            return 1;
        }
#ifdef DEBUG
        // printf("[event_loop_server] Handle %d event\n", rval);
#endif /* ifdef DEBUG */
    }

    return 0;
}

void client_socket_handle(struct event_loop_t* el, int client_fd, 
        void *client_data) {
    struct message_t* msg = (struct message_t*) client_data;
    if (msg == NULL) {
        printf("[client_socket_handle] no message allocated for client %d\n",
                client_fd);
        unregister_event(el, client_fd, E_READABLE | E_WRITEABLE);
        close(client_fd);
        return;
    }

    int nread = read(client_fd, msg->buffer, RECV_BUF_SIZE * sizeof(char));
    if (nread == -1) {
#ifdef DEBUG
        printf("[client_socket_handle] client %d read error: %s\n", client_fd,
                strerror(errno));
#endif /* ifdef DEBUG */
        unregister_event(el, client_fd, E_READABLE | E_WRITEABLE);
        free(client_data);
        close(client_fd);
        return;
    }

    if (nread == 0) {
#ifdef DEBUG
        printf("[client_socket_handle] (%s) client %d disconnect\n", msg->addrress,
                client_fd);
#endif /* ifdef DEBUG */
        unregister_event(el, client_fd, E_READABLE | E_WRITEABLE);
        free(client_data);
        close(client_fd);
        return;
    }

    printf("[client_socket_handle] (%s) client %d data: %s\n", msg->addrress,
            client_fd, msg->buffer);
}

void server_socket_handle(struct event_loop_t* el, int server_fd, 
        void *_) {
    int client_fd = -1;
    socklen_t addr_size = 0;
    struct sockaddr_storage client_addr = { 0 };

    addr_size = sizeof client_addr;
    client_fd = accept(server_fd, (struct sockaddr *) &client_addr, 
            &addr_size);
    if (client_fd == -1) {
        printf("[server_socket_handle] accept error: %s", strerror(errno));
        return;
    }

    struct message_t* msg = calloc(1, sizeof(struct message_t));
    if (msg == NULL) {
        printf("[server_socket_handle] message_t calloc error\n");
        close(client_fd);
        return;
    }

    inet_ntop(
            client_addr.ss_family,
            get_in_addr((struct sockaddr *) &client_addr),
            msg->addrress,
            sizeof msg->addrress);

    if (register_event(el, client_fd, E_READABLE, client_socket_handle, msg) 
            == ERANGE) {
        printf("[server_socket_handle] Maximum request handle capacity reached\n");
        close(client_fd);
        return;
    }

    printf("[server_socket_handle] New TCP connection from %s\n", msg->addrress);
}

int thread_pool_server(int server_fd) {
    struct thread_pool_t* tp;
    struct thread_work_t* tw;
    if ((tp = create_thread_pool()) == NULL) {
        printf("thread pool creation error\n");
        return 1;
    }


    /** Client Information */
    int client_fd = -1;
    char address[INET6_ADDRSTRLEN] = { 0 };
    socklen_t addr_size = 0;
    struct sockaddr_storage client_addr = { 0 };

    /** Accepting incoming TCP connections */
    while (1) {
        addr_size = sizeof client_addr;
        client_fd = accept(server_fd, (struct sockaddr *) &client_addr, 
                &addr_size);
        if (client_fd == -1) {
            printf("accept error: %s", strerror(errno));
            continue;
        }

        inet_ntop(
                client_addr.ss_family,
                get_in_addr((struct sockaddr *) &client_addr),
                address,
                sizeof address);

        printf("New TCP connection from %s\n", address);

        if (!enqueue_thread_work(tp, handle_client, client_fd)) {
            printf("Something went wrong when enqueuing thread work\n");
        }
    }

    return 0;
}

void handle_client(int client_fd, int lfd) {
    send(client_fd, "Hello World", strlen("Hello World"), 0);
    close(client_fd);
}
