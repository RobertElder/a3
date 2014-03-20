How to build librpc.a
---------------------

To build librpc.a, just do 'make'.

How to link with librpc.a
---------------------

If you want to link a program written in c with this library, you can compile with c, but you need to use g++ when linking because this library references uses the stl libraries:

Here is an example set of Makefile rules

client.o: client1.c
        gcc -w -c client1.c -o client.o
server_functions.o: server_functions.h server_functions.c
        gcc -g -c -o server_functions.o server_functions.c
server_function_skels.o: server_function_skels.h server_function_skels.c
        gcc -c -o server_function_skels.o server_function_skels.c
server.o: server.c
        gcc -w -c server.c -o server.o -pthread
server: server.o server_functions.o server_function_skels.o librpc.a
        g++ -w -L. server.o server_functions.o server_function_skels.o -lrpc -o server -pthread
client: librpc.a client1.o
        g++ -w -L. client1.o -lrpc -o client -pthread






