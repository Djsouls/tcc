all: client

client: client.c ../sockets/sockets.c
	gcc -Wall -Werror -o $@ client.c ../sockets/sockets.c -lpthread

persistent_client: persistent_client.c ../sockets/sockets.c
	gcc -Wall -Werror -o $@ persistent_client.c ../sockets/sockets.c -lpthread

run-client: client
	./client

run-persistent-client: persistent_client
	./persistent_client

clean:
	@rm -v client old_client
