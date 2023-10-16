#include "sockets.h"

int create_client_socket() {
    int status, client_fd; 

    client_fd = create_socket();
    status = connect_client(client_fd);

    if(status < 0) {
        return -1;
    }

    return client_fd;
}

int create_socket() {
    int client_fd;

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    return client_fd;
}

int connect_client(int client_fd) {
    int status;
    struct sockaddr_in serv_addr; 

    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT); 
  
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        return -1;
    }

    if ((status
        = connect(client_fd, (struct sockaddr*)&serv_addr,
                sizeof(serv_addr)))
        < 0) {
            perror("merda");
            printf("\nConnection Failed \n");
            return -1;
    }

    return status;
}