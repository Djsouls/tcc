#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h>

#include <arpa/inet.h>

#include <mtcp_api.h>
#include <mtcp_epoll.h>

 #include "../sockets/sockets.h"
#include "../common/common.h"

#define LATENCY_SIZE 10
#define SLEEP_TIME 10 // 10 microsseconds

#define MAX_CPUS 16

#define PORT 8888

struct thread_context {
    int core;
    mctx_t mctx;
};

void clean_buffer(char*);

void* client_thread(void*);
void* latency_thread(void*);

bool write_to_file();
void write_to_latency_file(FILE*, clock_t, clock_t);

void signal_handler(int);

struct thread_context* create_context(int);
int mtcp_create_connection(struct thread_context*);

bool done = false;

static char* conf_file;

static pthread_t threads[MAX_CPUS];

int main(int argc, char const* argv[])
{
    //int core_limit = 12;

    struct mtcp_conf mcfg;

    mtcp_getconf(&mcfg);
    mcfg.num_cores = 1;
    mtcp_setconf(&mcfg);

    conf_file = "client.conf";

    int ret = mtcp_init(conf_file);
    if(ret != 0) {
        fprintf(stderr, "DEU MERDA CONFIGURANDO O ARQUIVO");
        exit(EXIT_FAILURE);
    }

    // Register signal handling for gracefully shutdown
    mtcp_register_signal(SIGINT, signal_handler);
    
    // Initializes PRNG
    srandom(time(NULL));

    pthread_create(&threads[0], NULL, &latency_thread, NULL);
    pthread_join(threads[0], NULL);

/*
    for(int i = 1; i < core_limit + 1; i++) {
        pthread_create(&threads[i], NULL, &client_thread, NULL);
    }

    pthread_join(threads[0], NULL);
    for(int i = 1; i < core_limit + 1; i++) {
        printf("Joining %i\n", i);
        pthread_join(threads[i], NULL);
    }
*/
    printf("Properly exited. Bye bye\n");

    return 0;
}

void* latency_thread(void* arg) {
    struct thread_context* ctx = create_context(0);

    int mtcp_client_fd = mtcp_create_connection(ctx);

    printf("%i", ctx->core);
    printf("heh %i", mtcp_client_fd);
    //int client_fd = create_client_socket();

    char hello[BUFFER_SIZE];
    char hello_receive[BUFFER_SIZE];

    clock_t begin, end;

    FILE *file;
    file = fopen("latency.txt", "w");

    while(!done) {
        clean_buffer(hello);
        clean_buffer(hello_receive);

        memset(hello, 'b', BUFFER_SIZE);

        begin = clock();
        fprintf(stderr, "cucucucu\n");
        mtcp_write(ctx->mctx, mtcp_client_fd, hello, strlen(hello));
        fprintf(stderr, "oraora\n");
        mtcp_read(ctx->mctx, mtcp_client_fd, hello_receive, BUFFER_SIZE);
        fprintf(stderr, "eraeraera\n");
        end = clock();

        if(write_to_file()) {
            write_to_latency_file(file, begin, end);
        }

        usleep(SLEEP_TIME);
    }

    printf("Finishing latency thread\n");

    //close(client_fd);

    fclose(file);

    return 0;
}

int mtcp_create_connection(struct thread_context* ctx) {
    int client_fd;

    client_fd = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
    if(client_fd < 0) {
        fprintf(stderr, "Deu ruim criando o socket client...");
        return -1;
    }

    //mtcp_setsock_nonblock(ctx->mctx, client_fd);

    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("192.168.1.1");
    addr.sin_port = htons(PORT);

    int ret = mtcp_connect(ctx->mctx, client_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    if(ret < 0) {
        mtcp_close(ctx->mctx, client_fd);
        fprintf(stderr, "CUUUUUUUUUUUUUUU FALHOU NA CONEXAO");
        return -1;
    }

    return client_fd;
}

void* client_thread(void* arg) {
    int client_fd = create_client_socket();

    char hello[BUFFER_SIZE];
    char hello_receive[BUFFER_SIZE];

    while(!done) {
        clean_buffer(hello);
        clean_buffer(hello_receive);

        memset(hello, 'c', BUFFER_SIZE);

        send(client_fd, hello, strlen(hello), 0);
        recv(client_fd, hello_receive, BUFFER_SIZE, 0); 

        usleep(SLEEP_TIME);
    }

    printf("Finishing client thread\n");

    close(client_fd);

    return 0;
}

struct thread_context* create_context(int core) {
    struct thread_context* ctx;

    mtcp_core_affinitize(core);

    ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
    if(!ctx) {
        fprintf(stderr, "DEU MERDA ALOCANDO MEMÓRIA PRO CONTEXT");
        return NULL;
    }

    ctx->mctx = mtcp_create_context(core);
    if(!ctx->mctx) {
        fprintf(stderr, "DEU MERDA CRIANDO O CONTEXT MTCP CARAI\n");
        free(ctx);
        return NULL;
    }

    return ctx;
}

bool write_to_file() {
    long int magic_number = 50;
    long int random_number;

    random_number = random();

    return (random_number % magic_number) == 0;
}

void write_to_latency_file(FILE* file, clock_t begin, clock_t end) {
    double time_spent;

    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    time_spent *= 1000000;

    fprintf(file, "%f\n", time_spent);
}

void clean_buffer(char* buff) {
    memset(buff, '\0', BUFFER_SIZE);
}

void signal_handler(int sig) {
    printf("Handling signal %i...\n", sig);
    done = true;
}

