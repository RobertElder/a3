#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <vector>

#include "global_state.h"
#include "messages.h"
#include "rpc.h"

#define BACKLOG 10

using namespace std;

/*  Declared in global_state.h */
const char * context_str;

// socket fds to connect the server or the client to the binder
int server_to_binder_sockfd = -1;
int client_to_binder_sockfd = -1;

int server_max_fd = 0;
fd_set server_connection_fds;
fd_set server_listener_fds;
struct addrinfo * server_to_client_addrinfo = NULL;

struct addrinfo * client_sock_servinfo;

vector<struct func_skel_pair> registered_functions;

int argtypescmp(int * arg_types1, int * arg_types2, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (arg_types1[i] != arg_types2[i]) return -1;
    }
    return 0;
}

skeleton get_function_skeleton(struct function_prototype func) {
    for(unsigned int i = 0; i < registered_functions.size(); i++) {
        struct function_prototype temp = registered_functions[i].func;
        if (strcmp(temp.name, func.name) != 0) continue;
        if (temp.arg_len != func.arg_len) continue;
        if (argtypescmp(temp.arg_data, func.arg_data, func.arg_len) != 0) continue;
        return registered_functions[i].skel_function;
    }
    return 0;
}

// output: the socket file decriptor for a connection to the binder
int binder_socket_setup(){
    char * port = getenv ("BINDER_PORT");
    char * address = getenv ("BINDER_ADDRESS");

    struct addrinfo hints, *servinfo;
    int binder_sockfd;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    binder_sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (binder_sockfd == -1) {
        perror("Error in server: socket");
        return -1;
    }

    if (connect(binder_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(binder_sockfd);
        perror("Error in server: connect");
        return -1;
    }

    if (servinfo == NULL) {
        fprintf(stderr, "Server: failed to connect\n");
        return -1;
    }
    freeaddrinfo(servinfo);
    // socket successfuly created
    return binder_sockfd;
}

// set up socket to listen for incoming clients
int server_to_clients_setup() {
    int sockfd;
    struct addrinfo hints;
    int yes=1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, "0", &hints, &client_sock_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(server_to_client_addrinfo = client_sock_servinfo; server_to_client_addrinfo  != NULL; server_to_client_addrinfo = server_to_client_addrinfo->ai_next) {
        if ((sockfd = socket(server_to_client_addrinfo->ai_family, server_to_client_addrinfo->ai_socktype,
                server_to_client_addrinfo->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, server_to_client_addrinfo->ai_addr, server_to_client_addrinfo->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        socklen_t len = sizeof(*(server_to_client_addrinfo->ai_addr));
        if (getsockname(sockfd, ((struct sockaddr *)server_to_client_addrinfo->ai_addr), &len) == -1){
            perror("getsockname");
        }else{
            char * hostname = get_fully_qualified_hostname();
            //printf("Server available on %s\n", hostname);
            //printf("Server listening on %d\n", get_port_from_addrinfo(server_to_client_addrinfo));
            free(hostname);
            /* Flush the output so we can read it from the file */
            fflush(stdout);
        }
        break;
    }

    if (server_to_client_addrinfo == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    FD_SET(sockfd, &server_connection_fds);
    FD_SET(sockfd, &server_listener_fds);
    if(server_max_fd < sockfd) server_max_fd = sockfd;

    return 0;
}

int rpcInit() {
    FD_ZERO(&server_connection_fds);
    FD_ZERO(&server_listener_fds);

    server_to_binder_sockfd = binder_socket_setup();
    if (server_to_binder_sockfd < 0) {
        print_with_flush(context_str, "Failed to setup server connection to binder.\n");
        return -1;
    }
    /*  We want to continue to listen for messages from the binder */
    FD_SET(server_to_binder_sockfd, &server_connection_fds);
    server_max_fd = server_to_binder_sockfd;

    if (server_to_clients_setup()) {
        print_with_flush(context_str, "Failed to setup client litener.\n");
        return -1;
    }

    //printf("rpcInit has not been implemented yet.\n");
    // if get here, everything should have been set up correctly, return 0
    return 0;
};

// called by the client to execute a server function
// returns 0 if everything worked out
int rpcCall(char* name, int* argTypes, void** args) {
    //print_with_flush(context_str, "Client wants to execute function: %s\n", name);

    // connect to the binder if have not done so already
    if (client_to_binder_sockfd == -1) {
        client_to_binder_sockfd = binder_socket_setup();
        if (client_to_binder_sockfd < 0) {
            fprintf(stderr, "Failed to setup client connection to binder.\n");
            return FAIL_CONTACT_BINDER;
        }
    }

    int snd_status;

    // send a location req msg to the binder
    struct function_prototype f = create_function_prototype(name, argTypes);
    int function_prorotype_len = FUNCTION_NAME_LENGTH + sizeof(int) + sizeof(int) * f.arg_len;
    int * serialized_function_prototype = (int *)malloc(function_prorotype_len);
    serialize_function_prototype(f, serialized_function_prototype);

    struct message * out_msg = create_message_frame(
        function_prorotype_len , LOC_REQUEST, (int*)serialized_function_prototype);
    snd_status = send_message(client_to_binder_sockfd, out_msg);
    destroy_message_frame(out_msg);
    if (snd_status < 0) return FAIL_CONTACT_BINDER;

    // receive the server location for the procedure
    struct message * msg = recv_message(client_to_binder_sockfd);

    // if cannot get the location, return a negative value as a reson/error code
    if (msg->type == LOC_FAILURE) {
        // TODO: should return the reason code present in the data
        print_with_flush(context_str, "Cannot do rpcCall, LOC_FAILURE.\n");
        destroy_message_frame_and_data(msg);
        return NO_AVAILABLE_SERVER;
    }
    assert(msg->type == LOC_SUCCESS);

    // retrieve server identifier and port from the message
    struct location loc;
    memcpy(&loc, msg->data, sizeof(loc));
    //print_with_flush(context_str, "Client RPC call got server %s port %d.\n",loc.hostname, loc.port);
    char port_buffer[100] = {0};
    sprintf(port_buffer, "%d", loc.port);

    // send the server an execute req
    struct addrinfo hints, *servinfo;
    int rv;
    int client_to_server_sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(loc.hostname, port_buffer, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return FAIL_CONTACT_SERVER;
    }

    if ((client_to_server_sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
        servinfo->ai_protocol)) == -1) {
        perror("Error in client: socket");
        freeaddrinfo(servinfo);
        return FAIL_CONTACT_SERVER;
    }

    if (connect(client_to_server_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(client_to_server_sockfd);
        perror("Error in client: connect");
        freeaddrinfo(servinfo);
        return FAIL_CONTACT_SERVER;
    }

    if (servinfo == NULL) {
        fprintf(stderr, "Server: failed to connect\n");
        freeaddrinfo(servinfo);
        return FAIL_CONTACT_SERVER;
    }

    /*  Send a message saying we want to execute stuff */
    struct message * exec_msg = create_message_frame(0, EXECUTE, 0);
    snd_status = send_message(client_to_server_sockfd, exec_msg);
    destroy_message_frame_and_data(exec_msg);
    if (snd_status < 0) return FAIL_CONTACT_SERVER;

    /*  Send a message with the function prorotype */
    struct message * fcn_msg = create_message_frame(function_prorotype_len, FUNCTION_PROTOTYPE, serialized_function_prototype);
    snd_status = send_message(client_to_server_sockfd, fcn_msg);
    destroy_message_frame_and_data(fcn_msg);
    //print_function_prototype((char *)context_str, f);
    if (snd_status < 0) return FAIL_CONTACT_SERVER;

    /*  Send all the arguments */
    int * serialized_args = (int*)serialize_args(f, args);
    //print_args((char *)context_str, f, args);
    struct message * args_msg = create_message_frame(get_args_buffer_size(f), FUNCTION_ARGS, serialized_args);
    snd_status = send_message(client_to_server_sockfd, args_msg);
    destroy_message_frame_and_data(args_msg);
    if (snd_status < 0) return FAIL_CONTACT_SERVER;

    // wait for server to respond back
    struct message * return_msg = recv_message(client_to_server_sockfd);
    int return_code = 0;

    if (return_msg->type == FUNC_NOT_FOUND) return_code = SERVER_FUNC_NOT_FOUND;
    else if (return_msg->type == FUNC_FAILURE) return_code = FUNC_EXEC_FAILURE;

    deserialize_args(f, (char*)return_msg->data, args);
    destroy_message_frame_and_data(return_msg);

    free(f.arg_data);
    freeaddrinfo(servinfo);
    destroy_message_frame_and_data(msg);

    //print_with_flush(context_str, "Called %s port %d.\n",loc.hostname, loc.port);
    close(client_to_server_sockfd);

    return return_code;
};

int rpcCacheCall(char* name, int* argTypes, void** args){
    /*  This function differes from rpcCall in the following ways:
     * 1. rpcCacheCall caches the result of cache location request (i.e. mappings of function signature
     * and server locations) in a local database similar to the database maintained by the binder.
     * 2. For each request the client makes using rpcCacheCall, the local database would be scanned
     * for a server matching the function signature. If a server is found, the client would directly
     * call the server and receive the results. If the results are invalid or the server no longer exists,
     * it would send the request to the next server in the local cache and so on. If the request fails
     * on all servers in the local cache or there were none to begin with, it would transparently send
     * a location cache request to the binder and get an updated list of servers for that particular
     * function signature. It would cache the server locations and repeat the process by sending
     * request to one server at a time, resulting in either success if a server replies with the result
     * or failure in the case where all servers are exhausted without success.
     * The rpc library should only cache results and operate in this mode when the client uses rpc-
     * CacheCall. For the clients using rpcCall the behavior would be unchanged i.e. for every request,
     * the rpcCall wouldrst send the normal location request, get a server and then send the request to
     * that particular server.*/
    //printf("rpcCacheCall has not been implemented yet.\n");
    return -1;
};

// TODO: still missing various error checking and error code returns
int rpcRegister(char* name, int* argTypes, skeleton f){
    // get server location
    struct location loc;
    char * hostname = get_fully_qualified_hostname();
    memset(&loc.hostname, 0, HOSTNAME_BUFFER_LENGTH);
    memcpy(&loc.hostname, hostname, strlen(hostname) + 1);
    free(hostname);
    loc.port = get_port_from_addrinfo(server_to_client_addrinfo);

    // register the server with the binder by sending it its location
    struct message * out_msg = create_message_frame(sizeof(struct location), SERVER_REGISTER, (int*)&loc);
    send_message(server_to_binder_sockfd, out_msg);

    // register the function prorotype with the binder
    struct function_prototype pro = create_function_prototype(name, argTypes);
    out_msg->type = FUNCTION_PROTOTYPE;
    out_msg->length = FUNCTION_NAME_LENGTH + sizeof(int) + sizeof(int) * pro.arg_len;
    out_msg->data = (int*)malloc(out_msg->length);
    serialize_function_prototype(pro, out_msg->data);
    send_message(server_to_binder_sockfd, out_msg);

    // locally associate the server skeleton with the function prototype (name and arguments)
    struct func_skel_pair pair;
    pair.func = pro;
    pair.skel_function = f;
    registered_functions.push_back(pair);

    free(out_msg->data);
    destroy_message_frame(out_msg);

    // registration was successfull
    return 0;
};

int rpcExecute() {
    while(1) {
        struct message_and_fd m_and_fd = multiplexed_recv_message(
            &server_max_fd, &server_connection_fds, &server_listener_fds);

        // if the receive is due to a termination of the binder, want to clean up
        // if the receive is due to a termination of a client, want to continue receiving
        // (socket is already closed and removed by this point)
        if (!m_and_fd.message) {
            if (m_and_fd.fd == server_to_binder_sockfd) server_to_binder_sockfd = -1;
            continue;
        }

        struct message * in_msg = m_and_fd.message;
        switch (in_msg->type) {
            case SERVER_TERMINATE: {
                //print_with_flush(context_str, "Server got a message from binder to terminate.\n");
                destroy_message_frame_and_data(in_msg);
                goto exit;
                break;
            } case EXECUTE: {
                //print_with_flush(context_str, "Got a message from a client to execute.\n");
                struct message * prototype_msg = recv_message(m_and_fd.fd);
                struct message * args_msg = recv_message(m_and_fd.fd);
                struct function_prototype f = deserialize_function_prototype((int*)(prototype_msg->data));
                //print_function_prototype((char *)context_str, f);
                void ** args = create_empty_args_array(f);
                deserialize_args(f, (char*)args_msg->data, args);
                //print_args((char *)context_str, f, args);

                /*  Now find out which function we're actually calling and call it */
                int * arg_data_buffer = (int*) malloc((f.arg_len + 1) * sizeof(int));
                memcpy(arg_data_buffer, f.arg_data, sizeof(int) * f.arg_len);
                arg_data_buffer[f.arg_len] = 0;
                skeleton skel = get_function_skeleton(f);

                message_type msg_type = FUNCTION_ARGS;

                if (skel) {
                    int status = skel(arg_data_buffer, args);
                    if (status < 0) msg_type = FUNC_FAILURE;
                }

                /*  Serialize the arguments to send back the output */
                int * serialized_output = serialize_args(f, args);
                struct message * return_msg = create_message_frame(
                    get_args_buffer_size(f), skel ? msg_type : FUNC_NOT_FOUND, serialized_output);

                send_message(m_and_fd.fd, return_msg);
                destroy_message_frame_and_data(return_msg);

                free(arg_data_buffer);
                destroy_args_array(f, args);
                free(f.arg_data);
                destroy_message_frame_and_data(prototype_msg);
                destroy_message_frame_and_data(args_msg);
                break;
            } default: {
                assert(0);
            }
        }
        destroy_message_frame_and_data(in_msg);
    }

exit:
    for(unsigned int i = 0; i < registered_functions.size(); i++){
        free(registered_functions[i].func.arg_data);
    }
    freeaddrinfo(client_sock_servinfo);
    // close the binder connection that we created in rpcInit
    close(server_to_binder_sockfd);
    return 0;
};

// called by the client to gracefully terminate the system
// TODO: what are possible error conditions for this function?
int rpcTerminate(){
    // connect to the binder if have not done so already (ex. if not done any rpcCall)
    if (client_to_binder_sockfd == -1) {
        client_to_binder_sockfd = binder_socket_setup();
        if (client_to_binder_sockfd < 0) {
            fprintf(stderr, "Failed to setup client connection to binder.\n");
            return FAIL_CONTACT_BINDER;
        }
    }

    struct message * out_msg = create_message_frame(0, BINDER_TERMINATE, 0);
    send_message(client_to_binder_sockfd, out_msg);
    destroy_message_frame_and_data(out_msg);

    // close the connection since the binder will terminate once all the servers are terminated
    close(client_to_binder_sockfd);

    // termination happened successfully
    return 0;
}
