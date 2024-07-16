#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "config.h"
#include "ip.h"


int main() {
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	
    int server_fd = -1;
    if ((server_fd = setup()) == -1) {
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
        
        break;
    }

	return 0;
}
