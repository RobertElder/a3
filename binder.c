
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <assert.h>

#include "messages.h"

#define PORT "0"

#define BACKLOG 10

#define CONTEXT "Binder"

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void add_server(int ** server_sockets, int * num_server_sockets, int server_socket){
   *num_server_sockets = (*num_server_sockets) + 1;
   *server_sockets = (int*)realloc(*server_sockets, (*num_server_sockets) * sizeof(int));
   (*server_sockets)[(*num_server_sockets) - 1] = server_socket;
}

int main(void) {
    int num_server_sockets = 0;
    int * server_sockets = (int)0;
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes=1;
    int rv;

    int max_fd;
    fd_set client_fds;
    fd_set listener_fds;

    FD_ZERO(&client_fds);
    FD_ZERO(&listener_fds);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        socklen_t len = sizeof(*(p->ai_addr));
        if (getsockname(sockfd, ((struct sockaddr *)p->ai_addr), &len) == -1){
            perror("getsockname");
        } else {
            char * hostname = get_fully_qualified_hostname();
            printf("BINDER_ADDRESS %s\n", hostname);
            printf("BINDER_PORT %d\n", get_port_from_addrinfo(p));
            free(hostname);
            /* Flush the output so we can read it from the file */
            fflush(stdout);
        }
        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }
    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    FD_SET(sockfd, &client_fds);
    FD_SET(sockfd, &listener_fds);
    max_fd = sockfd;

    while(1) {
        struct message_and_fd m_and_fd = multiplexed_recv_message(&max_fd, &client_fds, &listener_fds);
        struct message * in_msg = m_and_fd.message;
        switch (in_msg->type) {
            case SERVER_HELLO: {
                print_with_flush(CONTEXT, "Got a hello message from a server.\n");
                add_server(&server_sockets, &num_server_sockets, m_and_fd.fd);
                break;
            } case SERVER_REGISTER: {
                struct location loc;
                memcpy(&loc, in_msg->data, sizeof(loc));

                print_with_flush(CONTEXT,
                    "Got a register message from server at %s, port %d.\n",
                    loc.hostname, loc.port);
                break;
            } case BINDER_TERMINATE: {
                print_with_flush(CONTEXT, "Got a message to terminate from a client.\n");
                /*  Terminate all the waiting servers */
                struct message * out_msg = create_message_frame(0, SERVER_TERMINATE, 0);
                int i;
                for (i = 0; i < num_server_sockets; i++){
                    send_message(server_sockets[i], out_msg);
                    FD_CLR(server_sockets[i], &client_fds);
                    close(server_sockets[i]);
                }
                destroy_message_frame_and_data(out_msg);
                /*  Clean up connection from client */
                FD_CLR(m_and_fd.fd, &client_fds);
                close(m_and_fd.fd);
                destroy_message_frame_and_data(in_msg);
                print_with_flush(CONTEXT, "Exiting binder...\n");
                free(server_sockets);

                return 0;
                break;
            } case LOC_REQUEST: {
                struct location * loc = (struct location*)malloc(sizeof(struct location));
                char hostname[HOSTNAME_BUFFER_LENGTH] = "test";
                memcpy(&(loc->hostname), hostname, HOSTNAME_BUFFER_LENGTH);
                loc->port = 10;

                struct message * out_msg = create_message_frame(
                    sizeof(struct location), LOC_SUCCESS, (int*)loc);
                send_message(m_and_fd.fd, out_msg);
                destroy_message_frame_and_data(out_msg);
                break;
            } default: {
                assert(0);
            }
        }
        destroy_message_frame_and_data(in_msg);
    }

    return 0;
}
