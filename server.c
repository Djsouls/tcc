#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <math.h>

#include <pthread.h>

#include <errno.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/epoll.h>

#include <netinet/in.h> 

#include "common.h"

#define PORT 8888
#define BACKLOG 4096

#define MAX_EVENTS 10000
#define EPOLL_TIMEOUT 3000 // 30 seconds

void* requests_counter_thread(void*);

bool connection_request(struct epoll_event, int);
bool input_request(struct epoll_event);
bool output_request(struct epoll_event);

int create_server_socket();

void create_connection();
void close_connection();
void clean_buffer(char*);
void print_message(char*);

int add_to_watchlist(int, int, int);
int delete_from_watchlist(int, int);

void signal_handler(int);

bool done = false;

int requests_counter = 0;

int main() {
    signal(SIGINT, signal_handler);

    pthread_t threads[1];
    pthread_create(&threads[0], NULL, &requests_counter_thread, NULL);

    int listener = create_server_socket();

    /* Make server socket non-blocking */
    fcntl(listener, F_SETFL, O_NONBLOCK);

    struct epoll_event event, incoming_events[MAX_EVENTS];
    int epoll_id, event_count;

    epoll_id = epoll_create1(0);
    if(epoll_id < 0) {
        perror("Error creating epoll file descriptor");
        exit(EXIT_FAILURE);
    }

    if(add_to_watchlist(epoll_id, listener, EPOLLIN) < 0) {
        perror("Error creating initial listener epoll_ctl");
        exit(EXIT_FAILURE);
    }

    char receive_buffer[BUFFER_SIZE];

    while(!done) {
        event_count = epoll_wait(epoll_id, incoming_events, MAX_EVENTS, EPOLL_TIMEOUT);

        for(int i = 0; i < event_count; i++) {
            event = incoming_events[i];

            if(connection_request(event, listener)) {
                create_connection(epoll_id, listener);
            }
            else if(input_request(event)) { // Receiving data from an already stablished connection
                clean_buffer(receive_buffer);
                ssize_t bytes_read = recv(event.data.fd, receive_buffer, BUFFER_SIZE, 0);

                if(bytes_read <= 0) {
                    close_connection(event.data.fd, epoll_id);
                    continue;
                }

                // print_message(receive_buffer);

                send(event.data.fd, receive_buffer, BUFFER_SIZE, 0);

                requests_counter++;
            }
            else if((event.events & EPOLLERR) || (event.events & EPOLLHUP)) {
                printf("Deu ruim\n");
                close_connection(event.data.fd, epoll_id);
            }
        }
    }

    close(epoll_id);
    close(listener);

    pthread_join(threads[0], NULL);

    printf("Byebye\n");

    return 0;
}

/* Counts the number of requests every second */
void* requests_counter_thread(void* arg) {
    char count[10];
    int size;

    FILE *file;
    file = fopen("requests_per_second.txt", "w");

    while(!done) {
        size = (int)((ceil(log10(requests_counter))+1)*sizeof(char));
        snprintf(count, size, "%d", requests_counter);
        requests_counter = 0;

        count[9] = '\n';

        fwrite(count, 10, 1, file);

        sleep(1);
    }

    fclose(file);

    return 0;
}

/* In case is a new connection request */
bool connection_request(struct epoll_event ev, int listener_fd) {
    return ev.data.fd == listener_fd;
}

/* An input request, ie, socket can be read */
bool input_request(struct epoll_event ev) {
    return (ev.events & EPOLLIN) == EPOLLIN;
}

/* An output request, ie, socket can be written */
bool output_request(struct epoll_event ev) {
    return (ev.events & EPOLLOUT) == EPOLLOUT;
}

/* Add a socket to our epoll watch list */
int add_to_watchlist(int epoll_id, int socket_fd, int flags) {
    struct epoll_event ev;

    ev.data.fd = socket_fd;
    ev.events = flags;

    int status = epoll_ctl(epoll_id, EPOLL_CTL_ADD, socket_fd, &ev);
    if(status < 0) {
        perror("Error adding to watchlist");

        return status;
    }

    return 0;
}

/* Removes a socket from watch list*/
int delete_from_watchlist(int epoll_id, int socket_fd) {
    int status = epoll_ctl(epoll_id, EPOLL_CTL_DEL, socket_fd, NULL);
    if(status < 0) {
        perror("Error deleting from watchlist");

        return status;
    }

    return status;
}


void clean_buffer(char* buff) {
    memset(buff, '\0', BUFFER_SIZE);
}

/* Helper for printing the buffer message in readable format */
void print_message(char* buff) {
    printf("Message from client: ");
    for(int i = 0; i < BUFFER_SIZE; i++) {
        printf("%c", buff[i]);
    }
    printf("\n");
}

/* Creates a generic TCP non-blocking socket to be used as
   our primary listener
*/
int create_server_socket() {
    int listener, opt = 1;

    /* Setting up listener socket */
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0) {
        perror("Could not create server socket");
        exit(EXIT_FAILURE);
    }

    if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
        &opt, sizeof(opt)) == -1) {
    
        perror("Could not set socket options");
        close(listener);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    if(bind(listener, (struct sockaddr *) &server_address,
        sizeof(struct sockaddr)) == -1) {

        perror("Could not bind socket");
        close(listener);
        exit(EXIT_FAILURE);
    }

    if(listen(listener, BACKLOG) == -1) {
        perror("Could not listen on socket");
        close(listener);
        exit(EXIT_FAILURE);
    }

    return listener;
}

/* Create a non-blocking connection with an incoming client */
void create_connection(int epoll_id, int listener_fd) {
    printf("\nCreating connection...\n");

    int client;
    struct sockaddr_in new_addr;
    int addr_len = sizeof(struct sockaddr_in);

    while(true) {
        client = accept(listener_fd, (struct sockaddr*)&new_addr,
                    (socklen_t *)&addr_len);

        if (client < 0) {
            /* We have processed all incoming connections. */
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                break;
            } else {
                perror("Could not accept client connection");
                break;
            }
        }

        /* Make client socket non-blocking */
        fcntl(client, F_SETFL, O_NONBLOCK);

        if(add_to_watchlist(epoll_id, client, EPOLLIN) < 0) {
            break;
        }
    }
}

void close_connection(int client_fd, int epoll_id) {
    printf("Connection with client %i closed\n", client_fd);

    /* Remove from our watch list*/
    delete_from_watchlist(epoll_id, client_fd);

    /* Closes the socket */
    close(client_fd);
}

void signal_handler(int sig) {
    printf("Handling signal %i...\n", sig);
    done = true;
}
