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

struct message * recv_message(int sockfd){
    /*
    * Caller is required to de-allocate the pointer to the message, and the message data
    */

    struct message * received_message = create_message_frame();
    int bytes_received;
    
    printf("Attempting to receive message length data (%d bytes)...\n", (int)sizeof(int));
    if ((bytes_received = recv(sockfd, &(received_message->length), sizeof(int), 0)) == -1)
        perror("Error in recv_message getting length.");
    /*  Probably never going to happen, but make sure we got the whole int */
    assert(bytes_received == sizeof(int));
    received_message->length = ntohs(received_message->length);
    /*  If this assertion fails, then there is probably a problem with the messaging format */
    assert(received_message->length < 1000 && received_message->length  > -1);
    /*  The received_message data should be a bunch of ints, otherwise how do we convert to host order? */
    assert(received_message->length % sizeof(int) == 0);
    /*  Allocate space for the rest of the received_message */
    received_message->data = (int *) malloc(received_message->length);

    printf("Attempting to receive message type data (%d bytes)...\n", (int)sizeof(int));
    if ((bytes_received = recv(sockfd, &(received_message->type), sizeof(int), 0)) == -1)
        perror("Error in recv_message getting type.");
    /*  Probably never going to happen, but make sure we got the whole int */
    assert(bytes_received == sizeof(int));
    received_message->type = ntohs(received_message->type);

    if(received_message->length > 0){
        printf("Attempting to receive message data of length %d...\n",received_message->length);
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
    
    printf("Attempting to send message length data (%d bytes), value is %d...\n", (int)sizeof(int), message_to_send->length);
    if(send(sockfd, &message_length, sizeof(int), 0) == -1){
        perror("Error in send_message sending length.\n");
    }

    printf("Attempting to send message type data (%d bytes), value is %d...\n", (int)sizeof(int), message_to_send->type);
    if(send(sockfd, &message_type, sizeof(int), 0) == -1){
        perror("Error in send_message sending type.\n");
    }

    /*  The received_message data should be a bunch of ints, otherwise how do we convert to host order? */
    assert(message_to_send->length % sizeof(int) == 0);

    int i;
    for(i = 0; i < message_to_send->length % sizeof(int); i++){
        message_to_send->data[i] = htons(message_to_send->data[i]);
    }

    if(message_to_send->length > 0){
        printf("Attempting to send %d bytes of data...\n", message_to_send->length);
        if(send(sockfd, message_to_send->data, message_to_send->length, 0) == -1){
            perror("Error in send_message sending data.\n");
        }
    }
}

struct message * create_message_frame(){
    struct message * m = malloc(sizeof(struct message));
    m->length = 0;
    m->type = 0;
    m->data = (int *)0;
    return m;
}

void destroy_message_frame_and_data(struct message * m){
    if(m->data){
        free(m->data);
    }
    free(m);
}
