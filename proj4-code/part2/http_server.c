#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "connection_queue.h"
#include "http.h"

#define BUFSIZE 512
#define LISTEN_QUEUE_LEN 5
#define N_THREADS 5

int keep_going = 1;
const char *serve_dir;

void handle_sigint(int signo) {
    keep_going = 0;
}

void *thread_func(void *arg) {
    connection_queue_t *queue = (connection_queue_t *) arg;
    while(1) {
        int thread_fd = connection_dequeue(queue);
        if (thread_fd == -1) {
            // perror("Error dequeuing connection from queue");
            return (void *) -1;
        }
        char rec_name[BUFSIZE] = {0};
        if(read_http_request(thread_fd, rec_name) == -1) {
            fprintf(stderr, "read HTTP request failed");
            close(thread_fd);
            continue;
        }
        char buf[BUFSIZE] = {0};
        if(snprintf(buf, BUFSIZE, "%s%s", serve_dir, rec_name) < 0){
            perror("snprintf");
            close(thread_fd);
            continue;
        }
        if(write_http_response(thread_fd, buf) == -1) {
            fprintf(stderr, "write HTTP response failed");
        }
        close(thread_fd);
    }
    return NULL;
}

int main(int argc, char **argv) {
    connection_queue_t cqueue; //Init queue
    if(connection_queue_init(&cqueue) < 0){
        fprintf(stderr, "connection_queue_init failed");
        return 1;
    }
    // First command is directory to serve, second command is port
    if (argc != 3) {
        printf("Usage: %s <directory> <port>\n", argv[0]);
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        return 1;
    }
    // Uncomment the lines below to use these definitions:
    serve_dir = argv[1];
    const char *port = argv[2];

    struct sigaction sigact;
    sigact.sa_handler = handle_sigint;
    if(sigfillset(&sigact.sa_mask) < 0){
        perror("sigfillset");
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        return 1; 
    }
    sigact.sa_flags = 0; // Note the lack of SA_RESTART
    if(sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
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
    if(ret_val != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(ret_val));
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        return 1;
    }
    // Initialize socket file descriptor
    int sock_fd = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if(sock_fd == -1) {
        perror("socket");
        freeaddrinfo(server);
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        return 1;
    }
    // Bind socket to receive at a specific port
    if(bind(sock_fd, server->ai_addr, server->ai_addrlen) == -1) {
        perror("bind");
        freeaddrinfo(server);
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        close(sock_fd);
        return 1;
    }
    freeaddrinfo(server);
    // Designate socket as a server socket
    if(listen(sock_fd, LISTEN_QUEUE_LEN) == -1) {
        perror("listen");
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        close(sock_fd);
        return 1;
    }
    //Create Threads
    sigset_t mask;
    sigset_t old;
    sigfillset(&mask);
    if(sigprocmask(SIG_BLOCK, &mask, &old) < 0){
        perror("sigprocmask");
        connection_queue_shutdown(&cqueue);
        connection_queue_free(&cqueue);
        close(sock_fd);
        return 1;
    }
    //Signal handling
    int result; 
    int exit_code = 0;
    pthread_t worker_threads[N_THREADS]; //Thread pool
    for (int i = 0; i < N_THREADS; i++) {
        if((result = pthread_create(worker_threads + i, NULL, thread_func, &cqueue)) != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(result));
            connection_queue_shutdown(&cqueue);
            for(int j = 0; j < i; j++){
                pthread_join(worker_threads[j], NULL);
            }
            connection_queue_free(&cqueue);
            close(sock_fd);
            return 1;
        }
    }
    if(sigprocmask(SIG_SETMASK, &old, NULL) < 0){
        perror("sigprocmask");
        connection_queue_shutdown(&cqueue);
        for(int i = 0; i < N_THREADS; i++){
            pthread_join(worker_threads[i], NULL);
        }
        connection_queue_free(&cqueue);
        close(sock_fd);
        return 1;
    }
    //Threads created
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
        if(connection_enqueue(&cqueue, client_fd) < 0){
            fprintf(stderr, "connection_enqueue failed");
            exit_code = 1; 
        }
    }
    //Cleanup
    if(connection_queue_shutdown(&cqueue) < 0){
        fprintf(stderr, "connection_queue_shutdown failed\n");
        exit_code = 1;
    }
    for(int i = 0; i < N_THREADS; i++) {
        if((result = pthread_join(worker_threads[i], NULL)) != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(result));
            exit_code = 1;
        }
    }
    if(connection_queue_free(&cqueue) < 0){
        fprintf(stderr, "connection_queue_free failed\n");
        exit_code = 1; 
    }
    if(close(sock_fd) != 0) {
        perror("close");
        return 1;
    }
    return exit_code;
}