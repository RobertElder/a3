FLAGS=-Wall -Werror -g
librpc.a: librpc.o
	ar rcs librpc.a librpc.o
messages.o:  messages.c messages.h
	gcc $(FLAGS) -c -o messages.o messages.c
librpc.o:  rpc.c rpc.h
	gcc $(FLAGS) -c -o librpc.o rpc.c
client: librpc.a client1.c messages.o
	gcc $(FLAGS) -L. messages.o client1.c -lrpc -o client
binder: binder.h binder.c messages.o
	gcc $(FLAGS) -L. messages.o binder.c -lrpc -o binder
server_functions.o: server_functions.h server_functions.c
	gcc $(FLAGS) -g -c -o server_functions.o server_functions.c
server_function_skels.o: server_function_skels.h server_function_skels.c
	gcc $(FLAGS) -c -o server_function_skels.o server_function_skels.c
server: server.c server_functions.o server_function_skels.o librpc.a messages.o
	gcc $(FLAGS) -L. server.c server_functions.o server_function_skels.o messages.o -lrpc -o server
test: client binder server
	#  Start the binder and pipe the output to a file to a file so we can automatically set the environment variables
	./binder > binderoutput & 
	#  Give the binder a second to bind and get address and port
	sleep 1
	#  Need to set environment variables on same like as client invokation so subshell sees environment variables
	export SERVER_ADDRESS=`cat binderoutput | head -n 1 | sed 's/SERVER_ADDRESS //'` && export SERVER_PORT=`cat binderoutput | tail -n 1 | sed 's/SERVER_PORT //'` && ./server && ./client
	#  Kill any binder processes that are in the background
	./cleanup-processes.sh
clean:
	rm *.o *.a client binderoutput binder server

