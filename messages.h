enum message_type {
    SERVER_HELLO,
    SERVER_TERMINATE,
    SERVER_TERMINATE_ACKNOWLEDGED,
    BINDER_TERMINATE
};

struct message{
    /*  This is the length of the message body without the first 8 bytes for the type and length field */
    int length;
    enum message_type type;
    int * data;
};

struct message * recv_message(int);
void send_message(int, struct message *);
struct message * create_message_frame();
void destroy_message_frame_and_data(struct message *);
