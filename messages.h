enum { TERMINATE_SERVER } message_type;

struct message{
    /*  This is the length of the message body without the first 8 bytes for the type and length field */
    int length;
    int type;
    int * data;
};

struct message * recv_message(int);
void send_message(int, struct message *);
