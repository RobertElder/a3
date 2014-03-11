librpc.a: librpc.o
	ar rcs librpc.a librpc.o
librpc.o:  rpc.c rpc.h
	gcc -Wall -g -c -o librpc.o rpc.c
client: librpc.a client1.c
	gcc -L. -Wall -g client1.c -lrpc -o client
binder: binder.h binder.c
	gcc -L. -Wall -g binder.c -lrpc -o binder
test: client binder
	#  Start the binder and pipe the output to a file to a file so we can automatically set the environment variables
	./binder > binderoutput & 
	#  Give the binder a second to bind and get address and port
	sleep 1
	#  Need to set environment variables on same like as client invokation so subshell sees environment variables
	export SERVER_ADDRESS=`cat binderoutput | head -n 1 | sed 's/SERVER_ADDRESS //'` && export SERVER_PORT=`cat binderoutput | tail -n 1 | sed 's/SERVER_PORT //'` && ./client
	#  Kill any binder processes that are in the background
	./cleanup-processes.sh
clean:
	rm *.o *.a client binderoutput binder
