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

#include "global_state.h"
#include "messages.h"
#include "rpc.h"

#define BACKLOG 10


int server_to_binder_sockfd;
/*  Declared in global_state.h */
const char * context_str;

int server_max_fd;
fd_set server_connection_fds;
fd_set server_listener_fds;
struct addrinfo * server_to_client_addrinfo = NULL;

struct addrinfo * client_sock_servinfo;

int binder_socket_setup(char * port, char * address){
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

// set up socket to listen to clients
int server_to_clients_setup(){
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
        printf("BINDER_ADDRESS %s\n", hostname);
        printf("BINDER_PORT %d\n", get_port_from_addrinfo(server_to_client_addrinfo));
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
    if(server_max_fd > sockfd)
        server_max_fd = sockfd;

    return 0;
}

int rpcInit(){
    /*The server first calls rpcInit, which does two things. First, it creates a connection socket
     * to be used for accepting connections from clients. Secondly, it opens a connection to the binder,
     * this connection is also used by the server for sending register requests to the binder and is left
     * open as long as the server is up so that the binder knows the server is available. This set of
     * permanently open connections to the binder (from all the servers) is somewhat unrealistic, but
     * provides a straightforward mechanism for the binder to discover server termination. The signature
     * of rpcInit is
     * int rpcInit(void);
     * The return value is 0 for success, negative if any part of the initialization sequence was unsuccessful
     * (using dirent negative values for dirent error conditions would be a good idea).*/

    FD_ZERO(&server_connection_fds);
    FD_ZERO(&server_listener_fds);
    // TODO: should be BINDER port and address
    char * port = getenv ("BINDER_PORT");
    char * address = getenv ("BINDER_ADDRESS");
    //printf("Running rpcInit for server with binder BINDER_ADDRESS: %s\n", address);
    //printf("Running rpcInit for server with binder BINDER_PORT: %s\n", port);

    server_to_binder_sockfd = binder_socket_setup(port, address);
    if (server_to_binder_sockfd < 0) {
        print_with_flush(context_str, "Failed to setup connection to binder.\n");
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
    // if get here, everything should have been et up correctly, return 0
    return -1;
};

int rpcCall(char* name, int* argTypes, void** args){
    /* First, note that the integer returned is the result of executing the rpcCall function, not the
     * result of the procedure that the rpcCall was executing. That is, if the rpcCall failed (e.g. if there
     * was no server that provided the desired procedure), that would be indicated by the integer result.
     * For successful execution, the returned value should be 0. If you wish to indicate a warning, it
     * should be a number greater than 0. A severe error (such as no available server) should be indicated
     * by a number less than 0. The procedure that the rpcCall is executing is, therefore, not able to
     * directly return a value. However, it may do so via some argument.
     * The name argument is the name of the remote procedure to be executed. A procedure of this
     * name must have been registered with the binder.
     * The argTypes array species the types of the arguments, and whether the argument is an \input
     * to", \output from", or \input to and output from" the server. Each argument has an integer to
     * encode the type information. These will collectively form the argTypes array. Thus argTypes[0]
     * species the type information for args[0], and so forth.
     * The argument type integer will be broken down as follows. Therst byte will specify the
     * input/output nature of the argument. Specically, if therst bit is set then the argument is input
     * to the server. If the second bit is set the argument is output from the server. The remaining
     * 6 bits of this byte are currently undened and must be set to 0. The next byte contains argument
     * type information. The types are the standard C types, excluding the null terminated string for
     * simplicity.
     * #define ARG_CHAR 1
     * #define ARG_SHORT 2
     * #define ARG_INT 3
     * #define ARG_LONG 4
     * #define ARG_DOUBLE 5
     * #define ARG_FLOAT 6
     * In addition, we wish to be able to pass arrays to our remote procedure. The lower two bytes
     * of the argument type integer will specify the length of the array. Arrays are limited to a length of
     * 216. If the array size is 0, the argument is considered to be a scalar, not an array. Note that it is
     * expected that the client programmer will have reserved successcient space for any output arrays.
     * You may alsond useful the denitions
     * #define ARG_INPUT 31
     * #define ARG_OUTPUT 30
     * */
    //printf("rpcCall has not been implemented yet.\n");

    // send a location req msg to the binder
    // TODO: should be BINDER port and address
    char * port = getenv ("BINDER_PORT");
    char * address = getenv ("BINDER_ADDRESS");
    int binder_sockfd = binder_socket_setup(port, address);
    if (binder_sockfd < 0) {
        fprintf(stderr, "Failed to create a binder socket.\n");
        return -1;
    }

    int msg_length = FUNCTION_NAME_LENGTH + sizeof(int);
    char * buffer = (char*)malloc(msg_length);
    memset(buffer, 0, msg_length);
    memcpy(buffer, name, FUNCTION_NAME_LENGTH);

    struct message * out_msg = create_message_frame(
        msg_length, LOC_REQUEST, (int*)buffer);
    send_message(binder_sockfd, out_msg);
    destroy_message_frame_and_data(out_msg);

    // receive the server location for the procedure
    struct message * msg = recv_message(binder_sockfd);

    // if cannot get the location, return a negative value as a reson/error code
    if (msg->type == LOC_FAILURE) {
        // TODO: should return the reason code present in the data
        destroy_message_frame_and_data(msg);
        return -1;
    }

    assert(msg->type == LOC_SUCCESS);

    // retrieve server identifier and port from the message
    struct location loc;
    memcpy(&loc, msg->data, sizeof(loc));
    fprintf(stderr, "Received hostname: %s\n", loc.hostname);

    // send the server an execute req


    destroy_message_frame_and_data(msg);
    // return 0 after sending the req
    return -1;
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

int rpcRegister(char* name, int* argTypes, skeleton f){
    /* This function does two key things. It calls the binder, informing it that a server procedure with the
    indicated name and list of argument types is available at this server. The result returned is 0 for a
    successful registration, positive for a warning (e.g., this is the same as some previously registered
    procedure), or negative for failure (e.g., could not locate binder). The function also makes an entry
    in a local database, associating the server skeleton with the name and list of argument types. The
rst two parameters are the same as those for the rpcCall function. The third parameter is the
    address of the server skeleton, which corresponds to the server procedure that is being registered.
    The skeleton function returns an integer to indicate if the server function call executes correctly
    or not. In the normal case, it will return zero. In case of an error it will return a negative value
    meaning that the server function execution failed (for example, wrong arguments). In this case,
    the RPC library at the server side should return an RPC failure message to the client. */
    //printf("rpcRegister has not been implemented yet.\n");
    char * buffer = (char*)malloc(HOSTNAME_BUFFER_LENGTH + sizeof(int));

    char * hostname = get_fully_qualified_hostname();
    int port = get_port_from_addrinfo(server_to_client_addrinfo);
    memcpy(buffer, hostname, HOSTNAME_BUFFER_LENGTH);
    memcpy(&(buffer[HOSTNAME_BUFFER_LENGTH]), &port, sizeof(int));

    struct message * out_msg = create_message_frame(HOSTNAME_BUFFER_LENGTH + sizeof(int), SERVER_REGISTER, (int*)buffer);
    send_message(server_to_binder_sockfd, out_msg);
    destroy_message_frame_and_data(out_msg);
    free(hostname);
    return -1;
};

int rpcExecute(){
    /* This function hands over control to the skeleton, which is expected to unmarshall the message, call the appro-
     * priate procedures as requested by the clients, and marshall the returns. Then rpcExecute sends the
     * result back to the client. It returns 0 for normally requested termination (the binder has requested
     * termination of the server) and negative otherwise (e.g. if there are no registered procedures to
     * serve).
     * rpcExecute should be able to handle multiple requests from clients without blocking, so that
     * a slow server function will not choke the whole server.*/

    struct message * out_msg = create_message_frame(0, SERVER_HELLO, 0);
    send_message(server_to_binder_sockfd, out_msg);
    destroy_message_frame_and_data(out_msg);

    while(1) {
        struct message_and_fd m_and_fd = multiplexed_recv_message(&server_max_fd, &server_connection_fds, &server_listener_fds);
        struct message * in_msg = m_and_fd.message;
        switch (in_msg->type){
            case SERVER_TERMINATE:{
                print_with_flush(context_str, "Got a message from binder to terminate.\n");
                destroy_message_frame_and_data(in_msg);
                goto exit;
                break;
	    }default:{
	        assert(0);
	    }
	}
    }

exit:
    freeaddrinfo(client_sock_servinfo);
    /*  Close the connection that we created in rpcInit */
    close(server_to_binder_sockfd);
    //printf("rpcExecute has not been implemented yet.\n");
    return -1;
};

int rpcTerminate(){
    /* To gracefully terminate the system a client executes the function:
     * int rpcTerminate ( void );
     * The client stub is expected to pass this request to the binder. The binder in turn will inform
     * the servers, which are all expected to gracefully terminate. The binder should terminate after all
     * servers have terminated. Clients are expected to terminate on their own cognizance. */

    char * port = getenv ("BINDER_PORT");
    char * address = getenv ("BINDER_ADDRESS");
    //printf("Running rpcTerminate for client with binder BINDER_ADDRESS: %s\n", address);
    //printf("Running rpcTerminate for client with binder BINDER_PORT: %s\n", port);

    struct addrinfo hints, *servinfo;
    int rv;
    int client_to_binder_sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    if ((client_to_binder_sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
        servinfo->ai_protocol)) == -1) {
        perror("Error in client: socket");
        freeaddrinfo(servinfo);
        return 1;
    }

    if (connect(client_to_binder_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(client_to_binder_sockfd);
        perror("Error in client: connect");
        freeaddrinfo(servinfo);
        return 1;
    }

    if (servinfo == NULL) {
        fprintf(stderr, "Server: failed to connect\n");
        freeaddrinfo(servinfo);
        return 1;
    }

    struct message * out_msg = create_message_frame(0, BINDER_TERMINATE, 0);
    send_message(client_to_binder_sockfd, out_msg);
    destroy_message_frame_and_data(out_msg);
    close(client_to_binder_sockfd);

    freeaddrinfo(servinfo);

    //printf("rpcTerminate has not been implemented yet.\n");
    return -1;
};
