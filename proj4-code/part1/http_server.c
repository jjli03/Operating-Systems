#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5

int keep_going = 1;

void handle_sigint(int signo) {
    keep_going = 0;
}

int main(int argc, char **argv) {
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    const char *serve_dir = argv[1];
    const char *port = argv[2];

    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    if(sigfillset(&sigact.sa_mask) < 0){
        perror("sigfillset");
        return 1;
    }
    sigact.sa_flags = 0; // Note the lack of SA_RESTART
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; //acting as a server
    struct addrinfo *server;

    // Set up address info for socket() and connect()
    int ret_val = getaddrinfo(NULL, port, &hints, &server);
    if (ret_val != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        return 1;
    }
    // Initialize socket file descriptor
    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if (sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        return 1;
    }
    // Bind socket to receive at a specific port
    if (bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);
    // Designate socket as a server socket
    if(listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        close(sock_fd);
        return 1;
    }
    //Accept loop
    while(keep_going != 0){
        int client_fd = accept(sock_fd, NULL, NULL);
        if(client_fd == -1) {
            if (errno != EINTR) {
                perror("accept");
                close(sock_fd);
                return 1;
            } else {
                break;
            }
        }
        char rec_name[BUFSIZE] = {0};
        if(read_http_request(client_fd, rec_name) < 0){
            fprintf(stderr, "read_http_request failed");
            close(client_fd);
            close(sock_fd);
            return 1;
        }
        char buf[BUFSIZE] = {0};
        if(snprintf(buf, BUFSIZE, "%s%s", serve_dir, rec_name) < 0){
            perror("snprintf");
            close(client_fd);
            close(sock_fd);
            return 1;
        }
        if(write_http_response(client_fd, buf) < 0){
            fprintf(stderr, "write_http_response failed");
            close(client_fd);
            close(sock_fd);
            return 1;
        }
        if(close(client_fd) == -1){
            perror("close client");
            return 1;
        }
    }
    if(close(sock_fd) == -1) {
        perror("close socket");
        return 1;
    }
    return 0;
}
