#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h>

#include "../sockets/sockets.h"
#include "../common/common.h"

#define LATENCY_SIZE 10
#define SLEEP_TIME 10 // 10 microsseconds

#define MAX_CPUS 16

void clean_buffer(char*);

void* client_thread(void*);
void* latency_thread(void*);

bool write_to_file();
void write_to_latency_file(FILE*, clock_t, clock_t);

void signal_handler(int);

bool done = false;

int main(int argc, char const* argv[])
{
    int core_limit = 12;

    pthread_t threads[MAX_CPUS];

    // Initializes PRNG
    srandom(time(NULL));

    // Register signal handling for gracefully shutdown
    signal(SIGINT, signal_handler);

    pthread_create(&threads[0], NULL, &latency_thread, NULL);

    for(int i = 1; i < core_limit + 1; i++) {
        pthread_create(&threads[i], NULL, &client_thread, NULL);
    }

    pthread_join(threads[0], NULL);
    for(int i = 1; i < core_limit + 1; i++) {
        printf("Joining %i\n", i);
        pthread_join(threads[i], NULL);
    }
    printf("Properly exited. Bye bye\n");

    return 0;
}

void* latency_thread(void* arg) {
    int client_fd = create_client_socket();

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

        send(client_fd, hello, strlen(hello), 0);
        recv(client_fd, hello_receive, BUFFER_SIZE, 0);

        end = clock();

        if(write_to_file()) {
            write_to_latency_file(file, begin, end);
        }

        usleep(SLEEP_TIME);
    }

    printf("Finishing latency thread\n");

    close(client_fd);

    fclose(file);

    return 0;
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
