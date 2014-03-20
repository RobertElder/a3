FLAGS=-Wall -Werror -g
librpc.a: rpc.o messages.o
	ar rcs librpc.a rpc.o
	ar rcs librpc.a messages.o
test: client binder server custom_client custom_server
	/bin/bash do-test.sh
rpc.o: rpc.cpp rpc.h
	g++ $(FLAGS) -c rpc.cpp -o rpc.o -pthread
messages.o: messages.cpp messages.h
	g++ $(FLAGS) -c messages.cpp -o messages.o
binder: binder.h binder.cpp librpc.a
	g++ $(FLAGS) -L. binder.cpp -lrpc -o binder
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
custom_client: librpc.a custom_client.cpp
	g++ $(FLAGS) -L. custom_client.cpp -lrpc -o custom_client -pthread
custom_server_functions.o: custom_server_functions.h custom_server_functions.cpp
	g++ $(FLAGS) -g -c -o custom_server_functions.o custom_server_functions.cpp
custom_server_function_skels.o: custom_server_function_skels.h custom_server_function_skels.cpp
	g++ $(FLAGS) -c -o custom_server_function_skels.o custom_server_function_skels.cpp
custom_server: custom_server.cpp custom_server_functions.o custom_server_function_skels.o librpc.a
	g++ $(FLAGS) -L. custom_server.cpp custom_server_functions.o custom_server_function_skels.o -lrpc -o custom_server -pthread
clean:
	rm *.o *.a client custom_client binderoutput binder server custom_server

