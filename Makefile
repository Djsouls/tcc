all: server

server: server.c
	gcc -Wall -Werror -o $@ server.c -lm -lpthread

client: client.c sockets.c
	gcc -Wall -Werror -o $@ client.c sockets.c -lpthread

run-server: server
	./server

run-client: client
	./client

clean:
	@rm -v server client
