#include "messages.h"
#include "rpc.h"
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
    *  Caller is required to de-allocate the pointer to the message, and the message data
    *  Returns NULL if the remote host closed the connection.
    */

    struct message * received_message = create_message_frame(0,(enum message_type)0,0);
    int bytes_received;

    //printf("Attempting to receive message length data (%d bytes)...\n", (int)sizeof(int));
    bytes_received = recv(sockfd, &(received_message->length), sizeof(int), 0);

    if (bytes_received <= 0) {
        // node on the other end terminated, return here to indicate that the connection should close
        destroy_message_frame(received_message);
        return 0;
    }

    /*  Probably never going to happen, but make sure we got the whole int */
    assert(bytes_received == sizeof(int));

    received_message->length = ntohl(received_message->length);

    /*  The received_message data should be a bunch of ints, otherwise how do we convert to host order? */
    assert(received_message->length % sizeof(int) == 0);

    /*  Allocate space for the rest of the received_message */
    received_message->data = (int *) malloc(received_message->length);

    //printf("Attempting to receive message type data (%d bytes)...\n", (int)sizeof(int));
    bytes_received = recv(sockfd, &(received_message->type), sizeof(int), 0);

    if (bytes_received <= 0) {
        // node on the other end terminated, return here to indicate that the connection should close
        destroy_message_frame(received_message);
        return 0;
    }

    if (bytes_received == -1) perror("Error in recv_message getting type.");
    assert(bytes_received != 0 && "Connection was closed by remote host.");

    /*  Probably never going to happen, but make sure we got the whole int */
    assert(bytes_received == sizeof(int));
    received_message->type = (enum message_type)ntohl(received_message->type);

    if(received_message->length > 0){
        bytes_received = recv(sockfd, received_message->data, received_message->length, 0);
        if (bytes_received <= 0) {
            // node on the other end terminated, return here to indicate that the connection should close
            destroy_message_frame(received_message);
            return 0;
        }

        /*  Probably never going to happen, but make sure we got the whole int */
        assert(bytes_received == received_message->length);

        u_int i;
        for (i = 0; i < received_message->length % sizeof(int); i++) {
            received_message->data[i] = ntohl(received_message->data[i]);
        }
    }
    return received_message;
}

// returns -1 if there is any problem sending the message
// returns 0 if the message is sent successfuly
int send_message(int sockfd, struct message * message_to_send){
    int message_length = htonl(message_to_send->length);
    int message_type = htonl(message_to_send->type);
    int bytes_sent;

    //printf("Attempting to send message length data (%d bytes), value is %d...\n", (int)sizeof(int), message_to_send->length);
    if ((bytes_sent = send(sockfd, &message_length, sizeof(int), 0)) == -1) {
        perror("Error in send_message sending length.\n");
        return -1;
    }

    assert(bytes_sent == sizeof(int));

    //printf("Attempting to send message type data (%d bytes), value is %d...\n", (int)sizeof(int), message_to_send->type);
    if ((bytes_sent = send(sockfd, &message_type, sizeof(int), 0)) == -1) {
        perror("Error in send_message sending type.\n");
        return -1;
    }

    assert(bytes_sent == sizeof(int));

    /*  The received_message data should be a bunch of ints, otherwise how do we convert to host order? */
    assert(message_to_send->length % sizeof(int) == 0);

    u_int i;
    for (i = 0; i < message_to_send->length % sizeof(int); i++) {
        message_to_send->data[i] = htonl(message_to_send->data[i]);
    }

    if (message_to_send->length > 0){
        //printf("Attempting to send %d bytes of data...\n", message_to_send->length);
        fflush(stdout);
        if((bytes_sent = send(sockfd, message_to_send->data, message_to_send->length, 0)) == -1){
            perror("Error in send_message sending data.\n");
            return -1;
        }

        assert(bytes_sent == message_to_send->length);
    }
    return 0;
}

struct message * create_message_frame(int len, enum message_type type, int * d){
    struct message * m = (struct message*)malloc(sizeof(struct message));
    m->length = len;
    m->type = type;
    m->data = (int *)d;
    return m;
}

void destroy_message_frame(struct message * m){
    free(m);
}

void destroy_message_frame_and_data(struct message * m){
    if(m->data){
        free(m->data);
    }
    destroy_message_frame(m);
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
                rtn.fd = i;
                rtn.message = recv_message(i);

                if (rtn.message){
                    return rtn;
                } else {
                    // the node on the other end of this socket terminated or went down
                    close(i);
                    FD_CLR(i, client_fds);
                    // inform caller of the termination in case clean up is necessary
                    return rtn;
                }
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
    char * buffer = (char*)malloc(buflen);
    memset(buffer,0,HOSTNAME_BUFFER_LENGTH);
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


void serialize_function_prototype(struct function_prototype f, int * buffer){
    /*  Flattens a function prototype struct (that includes a pointer) into a variable-length buffer */
    memcpy(buffer, &f.name, FUNCTION_NAME_LENGTH);
    memcpy(&buffer[FUNCTION_NAME_LENGTH / sizeof(int)], &f.arg_len, sizeof(int));
    memcpy(&buffer[FUNCTION_NAME_LENGTH / sizeof(int) + 1], f.arg_data, sizeof(int) * f.arg_len);
}

struct function_prototype deserialize_function_prototype(int * buffer){
    struct function_prototype f;
    memcpy(&f.name, buffer, FUNCTION_NAME_LENGTH);
    memcpy(&f.arg_len, &buffer[FUNCTION_NAME_LENGTH/sizeof(int)], sizeof(int));
    f.arg_data = (int*)malloc(sizeof(int) * f.arg_len);
    memcpy(f.arg_data, &buffer[FUNCTION_NAME_LENGTH/sizeof(int) + 1], sizeof(int) * f.arg_len);
    return f;
}

struct function_prototype create_function_prototype(char * name, int * argTypes){
    struct function_prototype f;
    int name_len = strlen(name);
    assert(name_len < FUNCTION_NAME_LENGTH);
    int i = 0;
    while(argTypes[i]){
        i++;
    }
    f.arg_len = i;
    f.arg_data = (int*)malloc(sizeof(int) * f.arg_len);
    memset(&f.name, 0, FUNCTION_NAME_LENGTH);
    memcpy(&f.name, name, name_len);
    memcpy(f.arg_data, argTypes, sizeof(int) * f.arg_len);
    return f;
}

void print_function_prototype(char * c, struct function_prototype f){
    print_with_flush(c, "-- Begin Function Prorotype --\n");
    print_with_flush(c, "name: %s\n",f.name);
    print_with_flush(c, "arg_len: %d\n",f.arg_len);
    for(int i = 0; i < f.arg_len; i++){
        print_with_flush(c, "argType %d: %X\n",i, f.arg_data[i]);
    }
    print_with_flush(c, "-- End Function Prorotype -- \n");
}

void print_one_args_array_size(char * context, int index, int data, void *p){
    int num_units = 0xFFFF & data;
    int is_array = 1;
    if(num_units == 0){
        num_units = 1;
        is_array = 0;
    }

    int data_type = ((0xFF << 16) & data) >> 16;

    if(is_array){
        print_with_flush(context, "%d) Array = {", index);
    }else{
        print_with_flush(context, "%d) Scalar: ", index);
    }
    switch(data_type){
        case ARG_CHAR:{
            for(int i = 0; i < num_units; i++){
                printf("%c", ((char *)p)[i]);
                if(i !=  (num_units-1) )
                    printf(", ");
            }
            break;
        }case ARG_SHORT:{
            for(int i = 0; i < num_units; i++){
                printf("%d", ((short *)p)[i]);
                if(i !=  (num_units-1) )
                    printf(", ");
            }
            break;
        }case ARG_INT:{
            for(int i = 0; i < num_units; i++){
                printf("%d", ((int*)p)[i]);
                if(i !=  (num_units-1) )
                    printf(", ");
            }
            break;
        }case ARG_LONG:{
            for(int i = 0; i < num_units; i++){
                printf("%ld", ((long int*)p)[i]);
                if(i !=  (num_units-1) )
                    printf(", ");
            }
            break;
        }case ARG_DOUBLE:{
            for(int i = 0; i < num_units; i++){
                printf("%f", ((double*)p)[i]);
                if(i !=  (num_units-1) )
                    printf(", ");
            }
            break;
        }case ARG_FLOAT:{
            for(int i = 0; i < num_units; i++){
                printf("%f", ((float *)p)[i]);
                if(i !=  (num_units-1) )
                    printf(", ");
            }
            break;
        }default:{
            assert(0);
        }
    }
    if(is_array){
        printf("}\n");
    }else{
        printf("\n");
    }
}

int get_one_args_array_size(int data){
    /*  Based on the data for one argument, how many bytes is the entire array for that arg? */

    /*  Mask out the data type and shift it back down to the first byte */
    int data_type = ((0xFF << 16) & data) >> 16;
    int unit_length;
    switch(data_type){
        case ARG_CHAR:{
            unit_length = sizeof(char);
            break;
        }case ARG_SHORT:{
            unit_length = sizeof(short);
            break;
        }case ARG_INT:{
            unit_length = sizeof(int);
            break;
        }case ARG_LONG:{
            unit_length = sizeof(long);
            break;
        }case ARG_DOUBLE:{
            unit_length = sizeof(double);
            break;
        }case ARG_FLOAT:{
            unit_length = sizeof(float);
            break;
        }default:{
            assert(0);
        }
    }
    /* Mask off last two bytes */
    int num_units = 0xFFFF & data;
    if(num_units == 0){
        /*  This is a special case, because 0 means scalar, which is the same as an array of one unit in terms of size. */
        num_units = 1;
    }
    return num_units * unit_length;
}

int get_args_buffer_size(struct function_prototype f){
    /*  For now, just serialize both input and output arguments.  We can have a flag to do one or the other later */

    int bytes_needed = 0;
    for(int i = 0; i < f.arg_len; i++){
        bytes_needed += get_one_args_array_size(f.arg_data[i]);
    }
    if(bytes_needed % sizeof(int) != 0){
        /*  Force 4-bytes alignment */
        return ((bytes_needed / sizeof(int)) * sizeof(int)) + sizeof(int);
    }else{
        return bytes_needed;
    }
}

int get_num_args_inputs(struct function_prototype f){
    /*  Returns the total number of input fields in the arg type array */
    int num = 0;
    for(int i = 0; i < f.arg_len; i++){
        if(f.arg_data[i] & (1 << ARG_INPUT)){
            num++;
        }
    }
    return num;
}

int get_num_args_outputs(struct function_prototype f){
    /*  Returns the total number of output fields in the arg type array */
    int num = 0;
    for(int i = 0; i < f.arg_len; i++){
        if(f.arg_data[i] & (1 << ARG_OUTPUT)){
            num++;
        }
    }
    return num;
}

void * get_nth_args_array(struct function_prototype f, void ** args, int n, int data_direction){
    int num = 0;
    for(int i = 0; i < f.arg_len; i++){
        if(f.arg_data[i] & (1 << data_direction)){
            if(num == n){
                return args[i];
            }
            num++;
        }
    }
    assert(0 && "nth thing not found");
    return 0;
}

void print_args(char * context, struct function_prototype f, void ** args){
    print_with_flush(context, "^^^^^^^^ BEGIN args ^^^^^^\n");
    for(int i = 0; i < f.arg_len; i++){
        print_one_args_array_size(context, i, f.arg_data[i], ((void**)args)[i]);
    }
    print_with_flush(context, "vvvvvvvv END args vvvvvvvv\n");
}

int * serialize_args(struct function_prototype f, void ** args){
    /*  Will return a buffer that contains both the input and output arguments pointed to by args */
    int size = get_args_buffer_size(f);
    char * buffer = (char*)malloc(size);
    memset(buffer, 0, size);
    int offset = 0;
    for(int i = 0; i < f.arg_len; i++){
        memcpy(&buffer[offset], ((void**)args)[i], get_one_args_array_size(f.arg_data[i]));
        offset += get_one_args_array_size(f.arg_data[i]);
    }
    return (int*)buffer;
}

void deserialize_args(struct function_prototype f, char * serialized_args, void ** deserialized_args, int output_or_input){
    /*  De-serialize a serialized args array into a pre-allocated array with the correct array lengths (like that found in an rpcCall) */
    int offset = 0;
    for(int i = 0; i < f.arg_len; i++){
        /*  Only do the copy on the input/out arguments or both if indicated */
        if(f.arg_data[i] & output_or_input){
            memcpy( ((void**)deserialized_args)[i], &serialized_args[offset], get_one_args_array_size(f.arg_data[i]));
        }
        offset += get_one_args_array_size(f.arg_data[i]);
    }
}

void ** create_empty_args_array(struct function_prototype f){
    /* allocate an empty args array that looks just like the args in rpcCall */
    void ** args = (void **)malloc(f.arg_len * sizeof(void *));
    for(int i = 0; i < f.arg_len; i++){
        args[i] = malloc(get_one_args_array_size(f.arg_data[i]));
        memset(args[i], 0, get_one_args_array_size(f.arg_data[i]));
    }
    return args;
}

void destroy_args_array(struct function_prototype f, void ** args){
    for(int i = 0; i < f.arg_len; i++){
        free(args[i]);
    }
    free(args);
}

// returns 0 if the two argTypes are the same
// returns -1 if the two argTypes are different
int argtypescmp(int * arg_types1, int * arg_types2, int len) {
    int i;
    for (i = 0; i < len; i++) {
        int arg1 = arg_types1[i];
        int arg2 = arg_types2[i];

        // parse the arg into type portion (first 2 bytes) and length (second 2 bytes)
        int type1 = 0xffff0000 & arg1;
        int len1 =  0x0000ffff & arg1;

        int type2 = 0xffff0000 & arg2;
        int len2 = 0x0000ffff & arg2;

        // compare that they are both arrays or both scalars
        if (!((len1 == 0 && len2 == 0) || (len1 > 0 && len2 > 0))) return -1;

        // compare the type portion
        if (type1 != type2) return -1;
    }
    return 0;
}

// returns 0 if the same; returns -1 if different
int compare_functions(struct function_prototype f1, struct function_prototype f2) {
    if (strcmp(f1.name, f2.name) != 0) return -1;
    if (f1.arg_len != f2.arg_len) return -1;
    if (argtypescmp(f1.arg_data, f2.arg_data, f1.arg_len) != 0) return -1;
    return 0;
}
