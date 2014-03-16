#include "messages.h"
#include <assert.h>
#include <stdlib.h>
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
#include <netdb.h>

#include <stdarg.h>

struct message * recv_message(int sockfd){
    /*
    * Caller is required to de-allocate the pointer to the message, and the message data
    */

    struct message * received_message = create_message_frame(0,0,0);
    int bytes_received;
    
    //printf("Attempting to receive message length data (%d bytes)...\n", (int)sizeof(int));
    if ((bytes_received = recv(sockfd, &(received_message->length), sizeof(int), 0)) == -1)
        perror("Error in recv_message getting length.");
    assert(bytes_received != 0 && "Connection was closed by remote host.");
    /*  Probably never going to happen, but make sure we got the whole int */
    assert(bytes_received == sizeof(int));
    received_message->length = ntohs(received_message->length);
    /*  If this assertion fails, then there is probably a problem with the messaging format */
    assert(received_message->length < 1000 && received_message->length  > -1);
    /*  The received_message data should be a bunch of ints, otherwise how do we convert to host order? */
    assert(received_message->length % sizeof(int) == 0);
    /*  Allocate space for the rest of the received_message */
    received_message->data = (int *) malloc(received_message->length);

    //printf("Attempting to receive message type data (%d bytes)...\n", (int)sizeof(int));
    if ((bytes_received = recv(sockfd, &(received_message->type), sizeof(int), 0)) == -1)
        perror("Error in recv_message getting type.");
    assert(bytes_received != 0 && "Connection was closed by remote host.");
    /*  Probably never going to happen, but make sure we got the whole int */
    assert(bytes_received == sizeof(int));
    received_message->type = ntohs(received_message->type);

    if(received_message->length > 0){
        //printf("Attempting to receive message data of length %d...\n",received_message->length);
        if ((bytes_received = recv(sockfd, received_message->data, received_message->length, 0)) == -1){
            perror("Error in recv_message getting data.");
        }
        /*  0 means closed connection */
        assert(bytes_received != 0);
        /*  Probably never going to happen, but make sure we got the whole int */
        assert(bytes_received == received_message->length);
        int i;
        for(i = 0; i < received_message->length % sizeof(int); i++){
            received_message->data[i] = ntohs(received_message->data[i]);
        }
    }
    return received_message;
}


void send_message(int sockfd, struct message * message_to_send){
    int message_length = htons(message_to_send->length);
    int message_type = htons(message_to_send->type);
    int bytes_sent;
    
    //printf("Attempting to send message length data (%d bytes), value is %d...\n", (int)sizeof(int), message_to_send->length);
    if((bytes_sent = send(sockfd, &message_length, sizeof(int), 0)) == -1){
        perror("Error in send_message sending length.\n");
    }

    assert(bytes_sent == sizeof(int));

    //printf("Attempting to send message type data (%d bytes), value is %d...\n", (int)sizeof(int), message_to_send->type);
    if((bytes_sent = send(sockfd, &message_type, sizeof(int), 0)) == -1){
        perror("Error in send_message sending type.\n");
    }

    assert(bytes_sent == sizeof(int));

    /*  The received_message data should be a bunch of ints, otherwise how do we convert to host order? */
    assert(message_to_send->length % sizeof(int) == 0);

    int i;
    for(i = 0; i < message_to_send->length % sizeof(int); i++){
        message_to_send->data[i] = htons(message_to_send->data[i]);
    }

    if(message_to_send->length > 0){
        //printf("Attempting to send %d bytes of data...\n", message_to_send->length);
        if((bytes_sent = send(sockfd, message_to_send->data, message_to_send->length, 0)) == -1){
            perror("Error in send_message sending data.\n");
        }

        assert(bytes_sent == message_to_send->length);
    }
}

struct message * create_message_frame(int len, enum message_type type, int * d){
    struct message * m = malloc(sizeof(struct message));
    m->length = len;
    m->type = type;
    m->data = (int *)d;
    return m;
}

void destroy_message_frame_and_data(struct message * m){
    if(m->data){
        free(m->data);
    }
    free(m);
}


struct message_and_fd multiplexed_recv_message(int * max_fd, fd_set * client_fds, fd_set * listener_fds){
    /*  max_fd is a pointer to an int that describes the max value of an fd in client_fds */
    /*  client_fds is the set of all fds that we want to monitor.  This includes listeners and clients */
    /*  listener_fds is the set of all fds that are waiting for new incomming connections */
    /*  This method will return the first message it gets from any client */
    struct sockaddr_storage remoteaddr;
    fd_set updated_fds;
    FD_ZERO(&updated_fds);
    while(1){
        updated_fds = *client_fds;
        if (select((*max_fd)+1, &updated_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        int i;
        for(i = 0; i <= *max_fd; i++) {
            /*  If we can read from it and it is a listener that accepts new connections */
            if (FD_ISSET(i, &updated_fds) && FD_ISSET(i, listener_fds) ) {
                //printf("Accepting incomming connection.\n");
                socklen_t addrlen = sizeof remoteaddr;
                int newfd = accept(i, (struct sockaddr *)&remoteaddr, &addrlen);

                if (newfd == -1) {
                    perror("accept");
                } else {
                    FD_SET(newfd, client_fds);
                    if (newfd > *max_fd) {
                        *max_fd = newfd;
                    }
                }
            /*  Data from an already open connection */
            }else if (FD_ISSET(i, &updated_fds)){
                //printf("Accepting incomming data.\n");
                struct message_and_fd rtn;
                rtn.message = recv_message(i);
                rtn.fd = i;
                return rtn;
            }
	}
    }
}

void print_with_flush(const char * context, const char * message, ...){
    va_list va;
    va_start(va, message);
    printf("%s: ", context);
    vprintf( (const char *)message, va );
    fflush(stdout);
    va_end(va);
}

char * get_fully_qualified_hostname(){
    /*  Caller is responsible for freeing memory returned */
    int buflen = HOSTNAME_BUFFER_LENGTH;
    char * buffer = malloc(buflen);
    struct hostent *hp;
    gethostname(buffer, buflen-1);
    hp = gethostbyname(buffer);
    sprintf(buffer, "%s", hp->h_name);
    return buffer;
}

int get_port_from_addrinfo(struct addrinfo * a){
    assert(a && a->ai_addr);
    return ntohs(((struct sockaddr_in*)a->ai_addr)->sin_port);
}
