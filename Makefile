all: server

server: server.c
	gcc -Wall -Werror -o $@ server.c

run: server
	./server

clean:
	@rm -v server