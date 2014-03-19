
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "rpc.h"

#define HOSTNAME_BUFFER_LENGTH 300
#define FUNCTION_NAME_LENGTH 300

enum message_type {
    SERVER_TERMINATE,
    BINDER_TERMINATE,
    SERVER_REGISTER,
    LOC_REQUEST,
    LOC_SUCCESS,
    LOC_FAILURE,
    EXECUTE,
    FUNCTION_PROTOTYPE,
    FUNCTION_ARGS
};

struct message{
    /*  This is the length of the message body without the first 8 bytes for the type and length field */
    int length;
    enum message_type type;
    int * data;
};

struct message_and_fd{
    struct message * message;
    int fd;
};

struct registration {
    char hostname[HOSTNAME_BUFFER_LENGTH];
    int port;
    /*  TODO:  add the*/
};

struct location {
    char hostname[HOSTNAME_BUFFER_LENGTH];
    int port;
};

struct location_request {
    /*  TODO:  add the*/
    int asdfasdf;
};

struct function_prototype {
    char name[FUNCTION_NAME_LENGTH];
    int arg_len;
    int * arg_data;
};

struct server {
    int sockfd;
    struct location loc;
};

struct func_loc_pair {
    struct server serv;
    struct function_prototype func;
};

struct func_skel_pair {
    struct function_prototype func;
    skeleton skel_function;
};

struct message * recv_message(int);
void send_message(int, struct message *);
struct message * create_message_frame(int, enum message_type, int *);
void destroy_message_frame_and_data(struct message *);
void destroy_message_frame(struct message *);
struct message_and_fd multiplexed_recv_message(int *, fd_set *, fd_set *);
void print_with_flush(const char * , const char * , ...);
char * get_fully_qualified_hostname();
int get_port_from_addrinfo(struct addrinfo *);
void serialize_function_prototype(struct function_prototype, int *);
void print_function_prototype(char *, struct function_prototype);
struct function_prototype deserialize_function_prototype(int * );
struct function_prototype create_function_prototype(char *, int *);  /*  Translate encapsulate in something that we can put in a vector */


int get_one_args_array_size(int);
int get_args_buffer_size(struct function_prototype);
int * serialize_args(struct function_prototype, void **);
void deserialize_args(struct function_prototype , char * , void ** );

void destroy_args_array(struct function_prototype , void ** );
void ** create_empty_args_array(struct function_prototype );
void print_args(char * , struct function_prototype , void **);
int get_num_args_inputs(struct function_prototype f);
int get_num_args_outputs(struct function_prototype f);
void * get_nth_args_array(struct function_prototype f, void ** args, int n, int data_direction);
