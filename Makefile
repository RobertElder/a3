librpc.a: librpc.o
	ar rcs librpc.a librpc.o
librpc.o:  rpc.c rpc.h
	gcc -Wall -g -c -o librpc.o rpc.c
client: librpc.a client1.c
	gcc -L. -Wall -g client1.c -lrpc -o client
test: client
	./client
clean:
	rm *.o *.a client
