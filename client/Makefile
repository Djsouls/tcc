all: client

client: client.c ../sockets/sockets.c
	gcc -Wall -Werror -o $@ client.c ../sockets/sockets.c -lpthread

run-client: client
	./client

clean:
	@rm -v client
