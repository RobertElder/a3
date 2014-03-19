FLAGS=-Wall -Werror -g
test: client binder server
	/bin/bash do-test.sh
librpc.a: librpc.o
	ar rcs librpc.a librpc.o
messages.o:  messages.cpp messages.h
	g++ $(FLAGS) -c -o messages.o messages.cpp
librpc.o:  rpc.cpp rpc.h messages.o
	g++ $(FLAGS) -c -o librpc.o rpc.cpp
client: librpc.a client1.cpp messages.o
	g++ $(FLAGS) -L. messages.o client1.cpp -lrpc -o client
binder: binder.h binder.cpp messages.o
	g++ $(FLAGS) -L. messages.o binder.cpp -lrpc -o binder
server_functions.o: server_functions.h server_functions.cpp
	g++ $(FLAGS) -g -c -o server_functions.o server_functions.cpp
server_function_skels.o: server_function_skels.h server_function_skels.cpp
	g++ $(FLAGS) -c -o server_function_skels.o server_function_skels.cpp
server: server.cpp server_functions.o server_function_skels.o librpc.a messages.o
	g++ $(FLAGS) -L. server.cpp server_functions.o server_function_skels.o messages.o -lrpc -o server
clean:
	rm *.o *.a client binderoutput binder server

