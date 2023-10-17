all: server

server: server.c
	gcc -Wall -Werror -o $@ server.c -lm -lpthread

run-server: server
	./server

clean:
	@rm -v server
