librpc.a: librpc.o
	ar rcs librpc.a librpc.o
librpc.o:  librpc.c librpc.h
	gcc -Wall -g -c -o librpc.o librpc.c
client: librpc.a
	gcc -L. -Wall -g sampleclient.c -lrpc -o client
test: client
	./client
clean:
	rm *.o *.a client
