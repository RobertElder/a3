
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
vector<struct server> registered_servers;

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int get_recent_server_index(int sockfd) {
    u_int i;
    for (i = 0; i < recent_servers.size(); i++) {
        if (recent_servers[i] == sockfd) return i;
    }
    return -1;
}

int get_registered_server_index(int sockfd) {
    u_int i;
    for (i = 0; i < registered_servers.size(); i++) {
        if (registered_servers[i].sockfd == sockfd) return i;
    }
    return -1;
}

void update_run_order(int recent_index, int sockfd) {
    if (recent_index != -1) recent_servers.erase(recent_servers.begin() + recent_index);
    recent_servers.push_back(sockfd);
}

void update_run_order(int sockfd) {
    int index = get_recent_server_index(sockfd);
    update_run_order(index, sockfd);
}

struct location find_func_server(struct function_prototype func) {
    // iterate over the func to loc map to find all locs for the target func
    vector<struct server> matches;
    u_int i;
    for(i = 0; i < func_loc_map.size(); i++) {
        struct function_prototype temp = func_loc_map[i].func;
        if (compare_functions(temp, func) == -1) continue;
        matches.push_back(func_loc_map[i].serv);
    }
    int num_matched = matches.size();

    if (num_matched == 0) {
        struct location null_loc;
        null_loc.port = -1;
        return null_loc;
    }

    if (num_matched == 1) {
        update_run_order(matches[0].sockfd);
        return matches[0].loc;
    }

    // more than one server can execute the function
    // want to run the server that was least recently run or never run
    int recent_index = recent_servers.size();
    int match_index;

    for (i = 0; i < matches.size(); i++) {
        int index = get_recent_server_index(matches[i].sockfd);

        if (index < recent_index) {
            recent_index = index;
            match_index = i;
        }
        if (index == -1) break;
    }

    update_run_order(recent_index, matches[match_index].sockfd);
    return matches[match_index].loc;
}

void register_server(struct server serv) {
    // check if this server has already registered
    u_int i;
    for (i = 0; i < registered_servers.size(); i++) {
        if (serv.sockfd == registered_servers[i].sockfd) return;
    }
    // add server to registered servers
    registered_servers.push_back(serv);
}

void register_server_func(struct func_loc_pair pair) {
    // check if the server has registered this func before
    u_int i;
    for (i = 0; i < func_loc_map.size(); i++) {
        struct func_loc_pair p = func_loc_map[i];
        if (p.serv.sockfd != pair.serv.sockfd) continue;
        if (compare_functions(p.func, pair.func) == -1) continue;
        // if get here, then the server has already registered this function with the binder
        return;
    }
    // add the pair to the map if it is new
    func_loc_map.push_back(pair);
}

void terminate_server(int server_index) {
    struct server serv = registered_servers[server_index];

    // remove the registered server
    registered_servers.erase(registered_servers.begin() + server_index);
    // remove server recent run record
    int recent_index = get_recent_server_index(serv.sockfd);
    if (recent_index >= 0) recent_servers.erase(recent_servers.begin() + recent_index);
    // remove server's registered functions from func_loc_map
    u_int i;
    for(i = 0; i < func_loc_map.size(); i++) {
        if (func_loc_map[i].serv.sockfd == serv.sockfd) {
            free(func_loc_map[i].func.arg_data);
            func_loc_map.erase(func_loc_map.begin() + i);
            i -= 1; // decrement so that elements are not skipped
        }
    }
}

int main(void) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int yes=1;
    int rv;
    int termination_state = 0; // 0 == not terminating; 1 == terminating

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

        // if the receive is due to a termination of a server, want to clean up after it
        // if the receive is due to a termination of a client, want to continue receiving
        // (socket is already closed and removed by this point)
        if (!m_and_fd.message) {
            int index = get_registered_server_index(m_and_fd.fd);
            if (index >= 0) terminate_server(index);

            // exit if binder is in termination mode and there are no more regitered servers
            if (termination_state == 1 && registered_servers.size() == 0) return 0;

            continue;
        }

        if (termination_state && get_registered_server_index(m_and_fd.fd) == -1) {
            // close client connection since we are terminating
            FD_CLR(m_and_fd.fd, &client_fds);
            close(m_and_fd.fd);
            continue;
        }

        struct message * in_msg = m_and_fd.message;
        switch (in_msg->type) {
            case SERVER_REGISTER: {
                struct location loc;
                memcpy(&loc, in_msg->data, sizeof(struct location));

                // deserialize the next received message, which is a function prototype
                struct message * msg = recv_message(m_and_fd.fd);
                struct function_prototype func = deserialize_function_prototype(msg->data);

                // register the server
                struct server s;
                s.sockfd = m_and_fd.fd;
                s.loc = loc;
                register_server(s);

                // register the function for the server
                struct func_loc_pair pair;
                pair.serv = s;
                pair.func = func;
                register_server_func(pair);

                destroy_message_frame_and_data(msg);
                break;
            } case BINDER_TERMINATE: {
                // send termination messages to all connected servers
                struct message * out_msg = create_message_frame(0, SERVER_TERMINATE, 0);
                for(vector<struct server>::iterator it = registered_servers.begin(); it != registered_servers.end(); it++){
                    int sockfd = (*it).sockfd;
                    send_message(sockfd, out_msg);
                }
                destroy_message_frame_and_data(out_msg);

                // close connection from client
                FD_CLR(m_and_fd.fd, &client_fds);
                close(m_and_fd.fd);

                termination_state = 1;
                break;
            } case LOC_REQUEST: {
                // extract the function name and argTypes form the message
                struct function_prototype func = deserialize_function_prototype(in_msg->data);

                // look up the server that can execute the requested function
                struct location loc = find_func_server(func);

                if (loc.port == -1) {
                    // no server can service this request
                    struct message * msg = create_message_frame(0, LOC_FAILURE, 0);
                    send_message(m_and_fd.fd, msg);
                    destroy_message_frame(msg);
                    free(func.arg_data);
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
