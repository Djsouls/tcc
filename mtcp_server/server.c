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

#include <arpa/inet.h>

#include <sys/socket.h>
#include <sys/epoll.h>

#include <netinet/in.h> 

#include "../common/common.h"


/* --------- MTCP INCLUDES AND STUFF -------------*/
#include <mtcp_api.h>
#include <mtcp_epoll.h>
/* --------- END MTCP INCLUDES AND STUFF --------------*/

#define PORT 8888
#define BACKLOG 4096

#define MAX_EVENTS 10000
#define EPOLL_TIMEOUT 3000 // 30 seconds

#define RUNTIME  60 // 60 seconds

#define MAX_CPUS 32

struct thread_context {
    mctx_t mctx;
    int epoll_id;
};

void* requests_counter_thread(void*);

bool connection_request(struct epoll_event, int);
bool input_request(struct epoll_event);
bool output_request(struct epoll_event);

int create_server_socket();
int mtcp_create_server_socket(struct thread_context*);

void create_connection();
void close_connection();
void clean_buffer(char*);
void print_message(char*);

int add_to_watchlist(int, int, int);
int delete_from_watchlist(int, int);

void signal_handler(int);

struct thread_context* init_server_thread(int);

bool done = false;

int requests_counter = 0;

static char *conf_file = NULL;

static pthread_t threads[MAX_CPUS];


int main() {

 /* ---- Setting up MTCP ----*/
    struct mtcp_conf mcfg;
    mtcp_getconf(&mcfg);
    mcfg.num_cores = 1;
    mtcp_setconf(&mcfg);

    conf_file = "server.conf";
    int ret = mtcp_init(conf_file);
    if(ret != 0) {
        printf("DEU MERDA CARREGANDO ARGUIVO!!!!\n");
        exit(EXIT_FAILURE);
    }

    mtcp_register_signal(SIGINT, signal_handler);


 /* ---- Finished setting up MTCP ----*/


    pthread_create(&threads[0], NULL, &requests_counter_thread, NULL);

    mctx_t mctx;
    struct thread_context* ctx = init_server_thread(0);
    if(!ctx) {
        fprintf(stderr, "Failed to initialize server thread\n");
    }

    mctx = ctx->mctx;

    int mtcp_listener = mtcp_create_server_socket(ctx);

    int listener = create_server_socket();
    /* Make server socket non-blocking */
    fcntl(listener, F_SETFL, O_NONBLOCK);

    printf("%i\n", mtcp_listener);

    struct mtcp_epoll_event ev;
    struct mtcp_epoll_event mtcp_events[MAX_EVENTS];

    ev.events = MTCP_EPOLLIN;
    ev.data.sockid = mtcp_listener;
    mtcp_epoll_ctl(ctx->mctx, ctx->epoll_id, MTCP_EPOLL_CTL_ADD, mtcp_listener, &ev);

    int count = 0;
    while(!done) {
        count = mtcp_epoll_wait(mctx, ctx->epoll_id, mtcp_events, MAX_EVENTS, EPOLL_TIMEOUT);
        for(int i = 0; i < count; i++) {
            if(mtcp_events[i].data.sockid == mtcp_listener) {
                printf("Requisicao de conexaxo nessa porora");
            }

            printf("oia os event");
        }
        printf("Count caralho %i\n", count);
    }

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
    int measures = 0;

    FILE *file;
    file = fopen("requests_per_second.txt", "w");

    int diff = 0;
    int last_read = 0;
    while(!done) {
        diff = requests_counter - last_read;
        last_read = requests_counter;
 
        fprintf(file, "%i\n", diff);

        measures++;
        if(measures > RUNTIME + 1) done = true;

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

int create_server_socket() {
    int listener, opt = 1;

    /* Setting up listener socket */
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0) {
        perror("Could not create server socket");
        exit(EXIT_FAILURE);
    }

    int ret = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if(ret <  0) {
        perror("Could not set socket options to nonblocking");
        close(listener);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    //server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_addr.s_addr = inet_addr("10.10.2.1");
    server_address.sin_port = htons(PORT);

    ret = bind(listener, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in));

    if(ret < 0) {
        perror("Could not bind MTCP socket");
        close(listener);
        exit(EXIT_FAILURE);
    }

    ret = listen(listener, BACKLOG);
    if(ret < 0) {
        perror("Could not listen on MTPC socket");
        close(listener);
        exit(EXIT_FAILURE);
    }

    return listener;
}


/* Creates a generic TCP non-blocking socket to be used as
   our primary listener
*/
int mtcp_create_server_socket(struct thread_context* ctx) {
    int listener;

    /* Setting up listener socket */
    listener = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
    if(listener < 0) {
        perror("Could not create server socket");
        exit(EXIT_FAILURE);
    }

    int ret = mtcp_setsock_nonblock(ctx->mctx, listener);
    if(ret <  0) {
        perror("Could not set socket options to nonblocking");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET;
    //server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_addr.s_addr = inet_addr("10.10.2.1");
    server_address.sin_port = htons(PORT);

    ret = mtcp_bind(ctx->mctx, listener, 
                        (struct sockaddr *)&server_address, sizeof(struct sockaddr_in));

    if(ret < 0) {
        perror("Could not bind MTCP socket");
        exit(EXIT_FAILURE);
    }

    ret = mtcp_listen(ctx->mctx, listener, BACKLOG);
    if(ret < 0) {
        perror("Could not listen on MTPC socket");
        exit(EXIT_FAILURE);
    }

    return listener;
}


struct thread_context* init_server_thread(int core) {
    struct thread_context* ctx;
    
    mtcp_core_affinitize(core);

    ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
    if(!ctx) {
        fprintf(stderr, "DEU MERDA ALOCANDO MEMÓRIA PRO CONTEXT\n");
        return NULL;
    }

    ctx->mctx = mtcp_create_context(core);
    if(!ctx->mctx) {
        fprintf(stderr, "DEU MERDA CRIANDO O CONTEXT MTCP CARAI\n");
        free(ctx);
        return NULL;
    }

    ctx->epoll_id = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
    if(ctx->epoll_id < 0) {
        mtcp_destroy_context(ctx->mctx);
        free(ctx);
        fprintf(stderr, "DEU MERDA CRIANDO O EPOLL MTCP\n");
        return NULL;
    }

    return ctx;

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

