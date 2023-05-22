#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "http.h"

#define BUFSIZE 512

const char *get_mime_type(const char *file_extension) {
    if (strcmp(".txt", file_extension) == 0) {
        return "text/plain";
    } else if (strcmp(".html", file_extension) == 0) {
        return "text/html";
    } else if (strcmp(".jpg", file_extension) == 0) {
        return "image/jpeg";
    } else if (strcmp(".png", file_extension) == 0) {
        return "image/png";
    } else if (strcmp(".pdf", file_extension) == 0) {
        return "application/pdf";
    }

    return NULL;
}

int read_http_request(int fd, char *resource_name) {
    char buf[BUFSIZE];
    int sock_fd_copy = dup(fd);
    if(sock_fd_copy == -1) {
        perror("dup");
        return -1;
    }
    FILE *socket_stream = fdopen(sock_fd_copy, "r");
    if (socket_stream == NULL) {
        perror("fdopen");
        close(sock_fd_copy);
        return -1;
    }
    // Disable the usual stdio buffering
    if (setvbuf(socket_stream, NULL, _IONBF, 0) != 0) {
        perror("setvbuf");
        fclose(socket_stream);
        return -1;
    }
    if(fgets(buf, BUFSIZE, socket_stream) == NULL){
        perror("fgets");
        fclose(socket_stream);
        return -1;
    }
    //Copy method, name, and version from the first line
    char *method, *name, *version;
    if((method = strtok(buf, " ")) == NULL){
        perror("request failed");
        fclose(socket_stream);
        return -1;
    }
    if((name = strtok(NULL, " ")) == NULL){
        perror("request failed");
        fclose(socket_stream);
        return -1;
    }
    if((version = strtok(NULL, "\r\n")) == NULL){
        perror("request failed");
        fclose(socket_stream);
        return -1;
    }
    strcpy(resource_name, name); //Obtain and set name
    strcpy(buf, ""); //Clear first line from buffer
    while(fgets(buf, BUFSIZE, socket_stream) != NULL) { //Read the rest of the request till \r\n
        if(strcmp(buf, "\r\n") == 0){
            break;
        }
    }
    if(ferror(socket_stream) != 0) { //Error handling for fgets while loop
        perror("fgets or while loop error");
        fclose(socket_stream);
        return -1;
    }
    if(fclose(socket_stream) != 0) { //Close copy socket fd
        perror("fclose");
        return -1;
    }
    return 0;
}

int write_http_response(int fd, const char *resource_path) {
    char buf[BUFSIZE];
    const char *file_type = "";
    struct stat detail;
    if(stat(resource_path, &detail) < 0){
        //404 case
        const char *no_file = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        if(write(fd, no_file, strlen(no_file)) < 0) {
            perror("write");
            return -1;
        }
        return 0;
    }
    //202 case
    if((file_type = get_mime_type(strrchr(resource_path, '.'))) == NULL){
        fprintf(stderr, "Unidentified file type"); 
        return -1;
    }
    if(snprintf(buf, BUFSIZE, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", file_type, (long) detail.st_size) < 0){
        perror("snprintf");
        return -1;
    }
    if(write(fd, buf, strlen(buf)) < 0){
        perror("write");
        return -1;
    }
    strcpy(buf, ""); //Clear header from buffer
    //header complete
    int bytes_read;
    int file = open(resource_path, O_RDONLY);
    if(file < 0){
        perror("open");
        return -1;
    }
    while((bytes_read = read(file, buf, BUFSIZE)) > 0) {
        if(write(fd, buf, bytes_read) < 0) {
            perror("write");
            close(file);
            return -1;
        }
    }
    if(bytes_read < 0) {
        perror("read");
        close(file);
        return -1;
    }
    if(close(file) < 0) {
        perror("close");
        return -1;
    }
    return 0;
}
