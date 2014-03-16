
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define HOSTNAME_BUFFER_LENGTH 300
#define FUNCTION_NAME_LENGTH 300

enum message_type {
    SERVER_HELLO,
    SERVER_TERMINATE,
    BINDER_TERMINATE,
    SERVER_REGISTER,
    LOC_REQUEST,
    LOC_SUCCESS,
    LOC_FAILURE,
    EXECUTE
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

struct location {
    char hostname[HOSTNAME_BUFFER_LENGTH];
    int port;
};

struct message * recv_message(int);
void send_message(int, struct message *);
struct message * create_message_frame(int, enum message_type, int *);
void destroy_message_frame_and_data(struct message *);
struct message_and_fd multiplexed_recv_message(int *, fd_set *, fd_set *);
void print_with_flush(const char * , const char * , ...);
char * get_fully_qualified_hostname();
int get_port_from_addrinfo(struct addrinfo *);
