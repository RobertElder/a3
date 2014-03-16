FLAGS=-Wall -Werror -g
test: client binder server
	/bin/bash do-test.sh
librpc.a: librpc.o
	ar rcs librpc.a librpc.o
messages.o:  messages.c messages.h
	g++ $(FLAGS) -c -o messages.o messages.c
librpc.o:  rpc.c rpc.h
	g++ $(FLAGS) -c -o librpc.o rpc.c
client: librpc.a client1.c messages.o
	g++ $(FLAGS) -L. messages.o client1.c -lrpc -o client
binder: binder.h binder.c messages.o
	g++ $(FLAGS) -L. messages.o binder.c -lrpc -o binder
server_functions.o: server_functions.h server_functions.c
	g++ $(FLAGS) -g -c -o server_functions.o server_functions.c
server_function_skels.o: server_function_skels.h server_function_skels.c
	g++ $(FLAGS) -c -o server_function_skels.o server_function_skels.c
server: server.c server_functions.o server_function_skels.o librpc.a messages.o
	g++ $(FLAGS) -L. server.c server_functions.o server_function_skels.o messages.o -lrpc -o server
clean:
	rm *.o *.a client binderoutput binder server

