/*
 * COMP30023 Computer System Assignment 1
 * Hongfei Yang <hongfeiy1@student.unimelb.edu.au>
 * 19/Apr/2018
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>


#define BUFFER_SIZE 1024        // size of buffer
#define MAX_REQ_LEN 50          // maximum request header length
#define MAX_FILE_PATH_LEN 1024  // maximum file path length
#define MAX_RES_CODE_LEN 20     // maximum response code length
#define MAX_RES_LEN 1024        // maximum response header length
#define MAX_CLIENT_NUM 1000000  // maximum number of allowed connections


/* struct to pack all arguments for worker thread */
struct arg_struct {
    int newsockfd;
    const char* fullPath;
};

/*
 * worker thread
 */
void *worker_thread(void* param) {


    char buffer[BUFFER_SIZE];
    
    // initialize buffer
    bzero(buffer, BUFFER_SIZE);

    // unpack args

    struct arg_struct* args = param;

    int newsockfd = args->newsockfd;

    char fullPath[MAX_FILE_PATH_LEN];

    strcpy(fullPath, args->fullPath);


    // create structure to store response code and response header
    char response_code[MAX_RES_CODE_LEN];
    bzero(response_code, MAX_RES_CODE_LEN);

    char response[MAX_RES_LEN];
    bzero(response, MAX_RES_LEN);


    // allow this thread to free its resources as soon as it finishes
    pthread_detach(pthread_self());

    // get the number of bytes read
    int n;
    n = read(newsockfd, buffer, BUFFER_SIZE);

    // must be able to read all request lines from socket
    if (n < 0) {
        perror("ERROR reading from socket");
        pthread_exit(NULL);
    }


    // copy the request code and file path from buffer

    char* request = strtok(buffer, " ");
    char* filePath = strtok(NULL, " ");

    // merge with root directory to form an absolute path for the file
    strcat(fullPath, filePath);

    // only serve GET request
    if (strcmp(request, "GET") != 0) {

        strcpy(response_code, "404 Bad Request");
        // parse response
        sprintf(response, "HTTP/1.0 %s\n\n", response_code);

        n = write(newsockfd, response, strlen(response));

        if (n < 0) {
            perror("ERROR writing to socket");
            pthread_exit(NULL);
        }

        // no need to open file, just go the the end of the code
        goto MARK;

    }


    // now, try to open this file, if this file is not able to be opened, then
    // respond with an error, otherwise check the file and service the client
    FILE *fp;
    fp = fopen(fullPath, "r");

    if (fp == NULL) {

        // file does not exist, respond with error code 404
        strcpy(response_code, "404 Not Found");

        // parse response
        sprintf(response, "HTTP/1.0 %s\n\n", response_code);

        n = write(newsockfd, response, strlen(response));

        if (n < 0) {
            perror("ERROR writing to socket");
            pthread_exit(NULL);
        }


    } else {

        // this file exist, so process
        strcpy(response_code, "200 OK");

        char filetype[MAX_FILE_PATH_LEN];

        bzero(filetype, MAX_FILE_PATH_LEN);

        // get file extension
        memcpy(filetype, strstr(filePath, "."), strlen(filePath));

        printf("string is %s, strlen is %ld\n",filetype, strlen(filetype));

        // then get the content type of this file to form MIME
        char content_type[MAX_FILE_PATH_LEN];
        bzero(content_type, MAX_FILE_PATH_LEN);

        if (strcmp(filetype, ".css") == 0) {
            strcpy(content_type, "text/css");
        } else if (strcmp(filetype, ".html") == 0) {
            strcpy(content_type, "text/html");
        } else if (strcmp(filetype, ".jpg") == 0) {
            strcpy(content_type, "image/jpeg");
        } else if (strcmp(filetype, ".js") == 0) {
            strcpy(content_type, "text/javascript");
        } else {
            perror("file type not recognized");
            pthread_exit(NULL);
            //strcpy(content_type, "text/pdf");
        }

        // get file stats, including file sizes (in byte)
        struct stat file_stat;

        if (stat(fullPath, &file_stat) < 0) {
            perror("get file stats failed");
            pthread_exit(NULL);
        }

        // parse the response header
        sprintf(response,
         "HTTP/1.0 %s\nContent-Length: %llu\nContent-Type: %s\n\n",
          response_code, (unsigned long long)file_stat.st_size, content_type);

        // tell the client that his request succeeds, the file is serving
        n = write(newsockfd, response, strlen(response));

        if (n < 0) {
            perror("ERROR writing to socket");
            pthread_exit(NULL);
        }

        // now read the file

        // read this file byte by byte, read each BUFFER_SIZE bytes and send to
        // our client, then read the next BUFFER_SIZE bytes and repeat

        int n_bytes_read;

        while (true) {

            // clear buffer each time before read as a precaution
            bzero(buffer, BUFFER_SIZE);

            // get the number of bytes read
            n_bytes_read = fread(&buffer, sizeof(char), BUFFER_SIZE, fp);

            if (n_bytes_read > 0) {
                // send to our client
                n = write(newsockfd, buffer, n_bytes_read);

                // check if send is successful
                if (n < 0) {
                    perror("ERROR writing to socket");
                    pthread_exit(NULL);
                }

            } else if (n_bytes_read == 0) {
                // this means that we have finished reading, so break the loop
                break;

            } else {

                // otherwise, fread returns -1, means fatal error, must
                // terminate now
                perror("ERROR reading file in fread");
                pthread_exit(NULL);
            }
        }
    }

    // everything is sent to our client, free up the param we just allocated

    MARK:

    free(param);

    close(newsockfd);

    printf("thread: finished serving %d\n", newsockfd);

    pthread_exit(NULL);
        
}



int main(int argc, char const *argv[]) {
    
    int sockfd, newsockfd; // socket file descriptor

    int sin_port; // Service Port number, a 16-bit port number in
                  // Network Byte Order, should be unsigned short

    struct sockaddr_in server_addr; // server address information

    struct protoent* protoent; // information of protocol

    // verify if command line input is valid
    if (argc < 3) {
        fprintf(stderr,"usage: ./<server_name> <port_number> <root_path>\n");
        exit(1);
    } else {
        sin_port = atoi(argv[1]);
    }

    // create a TCP socket file descriptor using the following parameters:
    // 
    // family       AF_INET (IPv4 protocols)
    // type         SOCK_STREAM (stream socket for TCP connection)
    // protocol     IPPROTO_TCP (TCP transport protocol), default to be 0
    protoent = getprotobyname("tcp");
    sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);

    // verify if the socket is successfully created
    if (sockfd == -1) {
        perror("ERROR opening socket");
        exit(1);
    }

 
    // initialize socket structure

    bzero((char*)&server_addr, sizeof(server_addr));

    // fill up socket address struct;

    server_addr.sin_family = AF_INET;

    // convert port number to Network Order Byte short
    server_addr.sin_port = htons(sin_port);

    // use my own IPv4 address
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // set this unused field to 0 as a precaution
    bzero(server_addr.sin_zero, sizeof(server_addr.sin_zero));


    // bind IP address and port number to the socket

    // cast a struct sockaddr_in to struct sockaddr because they
    // are parallel structure
    if (bind(sockfd, (struct sockaddr*)&server_addr, 
        sizeof(server_addr)) == -1) {
        
        perror("ERROR binding socket");
        close(sockfd);
        exit(1);
    }

    // listen for incoming connections,
    listen(sockfd, MAX_CLIENT_NUM);

    // run forever, accept incoming connections and
    // handle each to a worker thread
    while (true) {

        struct sockaddr_in client_addr;

        socklen_t client_len = sizeof(client_addr);

        // accept an incoming connection, create a new socket
        newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);

        // verify if the new socket file descriptor is valid
        if (newsockfd < 0) {
            // not valid, but no need to terminate;
            perror("ERROR opening client socket");
            continue;
        }


        // new socket is created, now handover to worker thread;

        // pack all arguments
        struct arg_struct* args = malloc(sizeof(*args));
        assert(args!=NULL);
        args->newsockfd = newsockfd;
        args->fullPath = argv[2];

        // create a worker thread and let it do the work

        pthread_t thread_id;

        printf("thread: handover to socket %d\n\n", newsockfd);


        // check thread creation
        if (pthread_create(&thread_id, NULL, worker_thread, args) != 0) {
            perror("thread creation failed");
            free(args);
            close(sockfd);
            close(newsockfd);
            pthread_exit(NULL);
        }

    }

    close(sockfd);

    return 0;
}