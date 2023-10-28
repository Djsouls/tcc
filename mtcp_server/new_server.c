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

#define PORT 80
#define BACKLOG 4096

#define MAX_EVENTS 10000
#define EPOLL_TIMEOUT -1 // 30 seconds

#define RUNTIME  115 // 60 seconds

#define MAX_CPUS 32

struct thread_context {
    mctx_t mctx;
    int epoll_id;
};

void* run_server(void*);
void* requests_counter_thread(void*);

int sum_requests();

bool connection_request(struct epoll_event, int);
bool input_request(struct epoll_event);
bool output_request(struct epoll_event);

int create_server_socket();
int mtcp_create_server_socket(struct thread_context*);

void mtcp_create_connection(struct thread_context*, int, int);
void mtcp_close_connection(struct thread_context*, int, int);

int handle_read(struct thread_context*, int, char*);

void clean_buffer(char*);
void print_message(char*);

int add_to_watchlist(int, int, int);
int delete_from_watchlist(int, int);

void signal_handler(int);

struct thread_context* init_server_thread(int);

bool done = false;

int requests_counter = 0;

static char *conf_file = NULL;

static pthread_t threads[MAX_CPUS + 1];

int requests_per_core[MAX_CPUS] = {0};

int main() {
    int n_servers = 5;
    int num_cores = (n_servers >= MAX_CPUS) ? MAX_CPUS : n_servers;

 /* ---- Setting up MTCP ----*/
    struct mtcp_conf mcfg;
    mtcp_getconf(&mcfg);
    mcfg.num_cores = num_cores;
    mtcp_setconf(&mcfg);

    conf_file = "server.conf";
    int ret = mtcp_init(conf_file);
    if(ret != 0) {
        printf("DEU MERDA CARREGANDO ARGUIVO!!!!\n");
        exit(EXIT_FAILURE);
    }

    mtcp_register_signal(SIGINT, signal_handler);


 /* ---- Finished setting up MTCP ----*/


    pthread_create(&threads[MAX_CPUS], NULL, &requests_counter_thread, NULL);

    int cores[MAX_CPUS];
    for(int i = 0; i < n_servers; i++) {
        cores[i] = i;
        pthread_create(&threads[i], NULL, &run_server, (void*) &cores[i]);
    }

    pthread_join(threads[MAX_CPUS], NULL);

    mtcp_destroy();

    printf("Byebye\n");

    return 0;
}

void* run_server(void* arg) {
    int core = *(int *)arg;

    mctx_t mctx;
    struct thread_context* ctx = init_server_thread(core);
    if(!ctx) {
        fprintf(stderr, "Failed to initialize server thread\n");
    }

    mctx = ctx->mctx;

    int mtcp_listener = mtcp_create_server_socket(ctx);

    struct mtcp_epoll_event ev;
    struct mtcp_epoll_event mtcp_events[MAX_EVENTS];

    ev.events = MTCP_EPOLLIN;
    ev.data.sockid = mtcp_listener;
    mtcp_epoll_ctl(ctx->mctx, ctx->epoll_id, MTCP_EPOLL_CTL_ADD, mtcp_listener, &ev);

    int count = 0;

    char receive_buffer[BUFFER_SIZE];

    struct mtcp_epoll_event event;

    printf("Ready to handle business\n");
    while(!done) {

        count = mtcp_epoll_wait(mctx, ctx->epoll_id, mtcp_events, MAX_EVENTS, EPOLL_TIMEOUT);

        for(int i = 0; i < count; i++) {
            event = mtcp_events[i];

            if(event.data.sockid == mtcp_listener) {
                mtcp_create_connection(ctx, ctx->epoll_id, mtcp_listener);

            } else if(event.events & MTCP_EPOLLIN) {
                clean_buffer(receive_buffer);

                int read = mtcp_read(ctx->mctx, event.data.sockid, receive_buffer, BUFFER_SIZE);

                if(read <= 0) {
                    mtcp_close_connection(ctx, event.data.sockid, ctx->epoll_id);
                    continue;
                }

                mtcp_write(ctx->mctx, event.data.sockid, receive_buffer, BUFFER_SIZE);

                requests_per_core[core]++;
            }
            else if((event.events & MTCP_EPOLLERR) || (event.events & MTCP_EPOLLHUP)) {
                if(errno != EAGAIN) {
                    printf("Cabo mesmo\n");
                    mtcp_close_connection(ctx, event.data.sockid, ctx->epoll_id);
                }
            }

        }
    }

    mtcp_close(ctx->mctx, mtcp_listener);

    return 0;
}

/* Counts the number of requests every second */
void* requests_counter_thread(void* arg) {
    int measures = 0;

    FILE *file;
    file = fopen("requests_per_second.txt", "w");

    int diff = 0;
    int last_read = 0;
    int requests_counter = 0;
    while(!done) {
        requests_counter = sum_requests();

        diff = requests_counter - last_read;
        last_read = requests_counter;
 
        fprintf(file, "%i\n", diff);

        measures++;
        if(measures > RUNTIME + 1) {
            done = true;
            printf("Stopping benchmark...\n");
        }

        sleep(1);
    }

    fclose(file);

    return 0;
}

int sum_requests() {
    int total = 0;
    for(int i = 0; i < MAX_CPUS; i++) {
        total += requests_per_core[i];
    }

    return total;
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
    server_address.sin_addr.s_addr = inet_addr("192.168.1.1");
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

void mtcp_create_connection(struct thread_context* ctx, int epoll_id, int listener_fd) {
    //printf("\nCreating MTCP connection...\n");

    int client;
    struct mtcp_epoll_event ev;

    while(true) {
        client = mtcp_accept(ctx->mctx, listener_fd, NULL, NULL);    
        
        if(client < 0) {
            /* We have processed all incomming connections. */
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                break;
            } else {
                perror("Could not accept client connection");
                break;
            }
        }

        ev.events = MTCP_EPOLLIN;
        ev.data.sockid = client;

        mtcp_setsock_nonblock(ctx->mctx, client);

        mtcp_epoll_ctl(ctx->mctx, epoll_id, MTCP_EPOLL_CTL_ADD, client, &ev);
    }
}

void mtcp_close_connection(struct thread_context* ctx, int client_fd, int epoll_id) {
    printf("Connection with client %i closed\n", client_fd);

    /* Remove from our watch list*/
    mtcp_epoll_ctl(ctx->mctx, epoll_id, MTCP_EPOLL_CTL_DEL, client_fd, NULL);

    /* Closes the socket */
    mtcp_close(ctx->mctx, client_fd);
}

void signal_handler(int sig) {
    printf("Handling signal %i...\n", sig);
    done = true;
}

