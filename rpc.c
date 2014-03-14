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

#include "global_state.h"
#include "rpc.h"
/*  Declared in global_state.h */
int server_to_binder_sockfd;

int rpcInit(){
    /*The server rst calls rpcInit, which does two things. First, it creates a connection socket
     * to be used for accepting connections from clients. Secondly, it opens a connection to the binder,
     * this connection is also used by the server for sending register requests to the binder and is left
     * open as long as the server is up so that the binder knows the server is available. This set of
     * permanently open connections to the binder (from all the servers) is somewhat unrealistic, but
     * provides a straightforward mechanism for the binder to discover server termination. The signature
     * of rpcInit is
     * int rpcInit(void);
     * The return value is 0 for success, negative if any part of the initialization sequence was unsuccessful
     * (using dirent negative values for dirent error conditions would be a good idea).*/

    char * port = getenv ("SERVER_PORT");
    char * address = getenv ("SERVER_ADDRESS");
    printf("Running rpcInit for server with binder SERVER_ADDRESS: %s\n", address);
    printf("Running rpcInit for server with binder SERVER_PORT: %s\n", port);

    struct addrinfo hints, *servinfo;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(address, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 0;
    }

    if ((server_to_binder_sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
        servinfo->ai_protocol)) == -1) {
        perror("Error in server: socket");
    }

    if (connect(server_to_binder_sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(server_to_binder_sockfd);
        perror("Error in server: connect");
        exit(1);
    }
    
    if (servinfo == NULL) {
        fprintf(stderr, "Server: failed to connect\n");
        return 0;
    }

    printf("rpcInit has not been implemented yet.\n");
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
     * The argument type integer will be broken down as follows. The rst byte will specify the
     * input/output nature of the argument. Specically, if the rst bit is set then the argument is input
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
     * You may also nd useful the denitions
     * #define ARG_INPUT 31
     * #define ARG_OUTPUT 30
     * */
    printf("rpcCall has not been implemented yet.\n");
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
     * the rpcCall would rst send the normal location request, get a server and then send the request to
     * that particular server.*/
    printf("rpcCacheCall has not been implemented yet.\n");
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
    printf("rpcRegister has not been implemented yet.\n");
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
    printf("rpcExecute has not been implemented yet.\n");
    return -1;
};

int rpcTerminate(){
    /* To gracefully terminate the system a client executes the function:
     * int rpcTerminate ( void );
     * The client stub is expected to pass this request to the binder. The binder in turn will inform
     * the servers, which are all expected to gracefully terminate. The binder should terminate after all
     * servers have terminated. Clients are expected to terminate on their own cognizance. */
    printf("rpcTerminate has not been implemented yet.\n");
    return -1;
};
