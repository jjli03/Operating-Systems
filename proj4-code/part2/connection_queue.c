#include <stdio.h>
#include <string.h>
#include "connection_queue.h"

int connection_queue_init(connection_queue_t *queue) {
    for(int i = 0; i < CAPACITY; i++){
        queue->client_fds[i] = -1;
    }
    queue->length = 0;
    queue->read_idx = 0;
    queue->write_idx = 0;
    queue->shutdown = 0;
    if(pthread_mutex_init(&queue->lock, NULL) != 0){
        fprintf(stderr, "pthread_mutex_init failed\n");
        return -1;
    }

    if(pthread_cond_init(&queue->queue_full, NULL) != 0){
        fprintf(stderr, "pthread_cond_init failed\n");
        pthread_mutex_destroy(&queue->lock);
        return -1;
    }

    if(pthread_cond_init(&queue->queue_empty, NULL) != 0){
        fprintf(stderr, "pthread_cond_init failed\n");
        pthread_mutex_destroy(&queue->lock);
        pthread_cond_destroy(&queue->queue_full);
        return -1;
    }
    return 0;
}

int connection_enqueue(connection_queue_t *queue, int connection_fd) {
    int result;
    if((result = pthread_mutex_lock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    while((queue->length == CAPACITY) && (queue->shutdown == 0)){ //Blocking
        if((result = pthread_cond_wait(&queue->queue_full, &queue->lock)) != 0){
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }
    if(queue->shutdown == 1){  
        pthread_mutex_unlock(&queue->lock);  
        return -1;  
    }
    //Set connection file descriptor
    queue->client_fds[queue->write_idx] = connection_fd;
    queue->write_idx = (queue->write_idx + 1) % CAPACITY;
    queue->length++; 

    if((result = pthread_cond_signal(&queue->queue_empty)) != 0){
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    if((result = pthread_mutex_unlock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }
    return 0;
}

int connection_dequeue(connection_queue_t *queue) {
    int result;
    int connection_fd;
    if((result = pthread_mutex_lock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    while((queue->length == 0) && (queue->shutdown == 0)){ //Blocking
        if((result = pthread_cond_wait(&queue->queue_empty, &queue->lock)) != 0){
            fprintf(stderr, "pthread_cond_wait: %s\n", strerror(result));
            pthread_mutex_unlock(&queue->lock);
            return -1;
        }
    }
    if(queue->shutdown == 1){  
        pthread_mutex_unlock(&queue->lock);  
        return -1;  
    }
    //Set queue index to connection file descriptor
    connection_fd = queue->client_fds[queue->read_idx];
    queue->read_idx = (queue->read_idx + 1) % CAPACITY;
    queue->length--;

    if((result = pthread_cond_signal(&queue->queue_full)) != 0){
        fprintf(stderr, "pthread_cond_signal: %s\n", strerror(result));
        pthread_mutex_unlock(&queue->lock);
        return -1;
    }
    if((result = pthread_mutex_unlock(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }
    return connection_fd;
}

int connection_queue_shutdown(connection_queue_t *queue) {
    int result;
    if((result = pthread_mutex_lock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(result));
        return -1;
    }
    queue->shutdown = 1;
    if((result = pthread_cond_broadcast(&queue->queue_full)) != 0){
        fprintf(stderr, "pthread_cond_broadcast (queue_full): %s\n", strerror(result));
        return -1;
    }
    if((result = pthread_cond_broadcast(&queue->queue_empty)) != 0){
        fprintf(stderr, "pthread_cond_broadcast (queue_empty): %s\n", strerror(result));
        return -1;
    }
    if((result = pthread_mutex_unlock(&queue->lock)) != 0){
        fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(result));
        return -1;
    }
    return 0;
}

int connection_queue_free(connection_queue_t *queue) {
    int result;
    if ((result = pthread_mutex_destroy(&queue->lock)) != 0) {
        fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(result));
        return -1;
    }
    if ((result = pthread_cond_destroy(&queue->queue_full)) != 0) {
        fprintf(stderr, "pthread_cond_destroy (queue_full): %s\n", strerror(result));
        return -1;
    }
    if ((result = pthread_cond_destroy(&queue->queue_empty)) != 0) {
        fprintf(stderr, "pthread_cond_destroy (queue_empty): %s\n", strerror(result));
        return -1;
    }
    return 0;
}
