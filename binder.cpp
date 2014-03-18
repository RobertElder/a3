
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
#include <vector>

#include "messages.h"

#define PORT "0"

#define BACKLOG 10

#define CONTEXT "Binder"

using namespace std;
vector<struct func_loc_pair> func_loc_map;
// recently run servers: from least recently run to most recently run
vector<int> recent_servers;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_server_index(int id) {
    u_int i;
    for (i = 0; i < recent_servers.size(); i++) {
        if (recent_servers[i] == id) return i;
    }
    return -1;
}

void update_run_order(int recent_index, int id) {
    if (recent_index != -1) recent_servers.erase(recent_servers.begin() + recent_index);
    recent_servers.push_back(id);
}

void update_run_order(int id) {
    int index = get_server_index(id);
    update_run_order(index, id);
}

int argtypescmp(int * arg_types1, int * arg_types2, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (arg_types1[i] != arg_types2[i]) return -1;
    }
    return 0;
}

struct location find_func_server(struct function_prototype func) {
    // iterate over the func to loc map to find all locs for the target func
    vector<struct server> matches;
    u_int i;
    for(i = 0; i < func_loc_map.size(); i++) {
        struct function_prototype temp = func_loc_map[i].func;
        if (strcmp(temp.name, func.name) != 0) continue;
        if (temp.arg_len != func.arg_len) continue;
        printf("%d %d", temp.arg_len, func.arg_len);
        fflush(stdout);
        if (argtypescmp(temp.arg_data, func.arg_data, func.arg_len) != 0) continue;
        matches.push_back(func_loc_map[i].serv);
    }
    int num_matched = matches.size();

    if (num_matched == 0) {
        struct location null_loc;
        null_loc.port = -1;
        return null_loc;
    }

    if (num_matched == 1) {
        update_run_order(matches[0].id);
        return matches[0].loc;
    }

    // more than one server can execute the function
    // want to run the server that was least recently run or never run
    int recent_index = recent_servers.size();
    int match_index;

    for (i = 0; i < matches.size(); i++) {
        int index = get_server_index(matches[i].id);

        if (index < recent_index) {
            recent_index = index;
            match_index = i;
        }
        if (index == -1) break;
    }

    update_run_order(recent_index, matches[match_index].id);
    return matches[match_index].loc;
}

int main(void) {
    vector<int> server_sockets;
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes=1;
    int rv;
    int next_serv_id = 0;

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
                //print_with_flush(CONTEXT, "Got a hello message from a server.\n");
                server_sockets.push_back(m_and_fd.fd);
                break;
            } case SERVER_REGISTER: {
                struct location loc;
                memcpy(&loc, in_msg->data, sizeof(struct location));
                print_with_flush(CONTEXT, "Got a register message from server at %s, port %d.\n", loc.hostname, loc.port);

                /*  Grab the next message which is a function prototype  */
                // deserialize the received message
                struct message * msg = recv_message(m_and_fd.fd);
                struct function_prototype func = deserialize_function_prototype(msg->data);

                // use the function prototype struct and pair it up with the location
                struct server s;
                s.id = next_serv_id;
                next_serv_id += 1;
                s.loc = loc;

                struct func_loc_pair pair;
                pair.serv = s;
                pair.func = func;
                func_loc_map.push_back(pair);
                destroy_message_frame_and_data(msg);
                break;
            } case BINDER_TERMINATE: {
                //print_with_flush(CONTEXT, "Got a message to terminate from a client.\n");
                /*  Terminate all the waiting servers */
                struct message * out_msg = create_message_frame(0, SERVER_TERMINATE, 0);
                for(vector<int>::iterator it = server_sockets.begin(); it != server_sockets.end(); it++){
                    send_message(*it, out_msg);
                    FD_CLR(*it, &client_fds);
                    close(*it);
                }
                destroy_message_frame_and_data(out_msg);
                /*  Clean up connection from client */
                FD_CLR(m_and_fd.fd, &client_fds);
                close(m_and_fd.fd);
                destroy_message_frame_and_data(in_msg);
                for(unsigned int i = 0; i < func_loc_map.size(); i++) {
                    free(func_loc_map[i].func.arg_data);
                }
                print_with_flush(CONTEXT, "Exiting binder...\n");
                return 0;
                break;
            } case LOC_REQUEST: {
                // extract the function name and argTypes form the message
                struct function_prototype func = deserialize_function_prototype(in_msg->data);

                // look up the server that can execute the requested function
                struct location loc = find_func_server(func);

                if (loc.port == -1) {
                    // no server can service this request
                    struct message * msg = create_message_frame(sizeof(int), LOC_FAILURE, (int*)-1);
                    send_message(m_and_fd.fd, msg);
                    destroy_message_frame(msg);
                    break;
                }

                // return the location to the client
                struct message * out_msg = create_message_frame(sizeof(struct location), LOC_SUCCESS, (int*)&loc);
                send_message(m_and_fd.fd, out_msg);
                destroy_message_frame(out_msg);
                free(func.arg_data);
                break;
            } default: {
                assert(0);
            }
        }
        destroy_message_frame_and_data(in_msg);
    }

    return 0;
}
