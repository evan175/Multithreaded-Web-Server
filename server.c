#include <sys/socket.h>
#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>

#define MAX_MSG_LEN 30
#define MAX_REPLY_LEN 300

struct thread_data{
    int server_socket;
    int client_socket;
};

int create_socket() {
    int socket_id;
    socket_id = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_id < 0) {
        printf("Error creating socket\n");
        printf("Errno: %d\n", errno);
    }

    return socket_id;
}

int bind_socket(char* ip, int port, int socket_id) {
    struct sockaddr_in bind_args;
    memset(&bind_args, 0, sizeof(bind_args));

    bind_args.sin_family = AF_INET;
    bind_args.sin_addr.s_addr = inet_addr(ip);
    bind_args.sin_port = htons(port);

    int rc = bind(socket_id, (struct sockaddr*) &bind_args, sizeof(bind_args));

    if(rc < 0) {
        printf("Error binding socket\n");
        printf("Errno: %d\n", errno);
    }

    return rc;
}

int accept_conns(int socket_id) {
    static struct sockaddr_in client_addr;
    int length = sizeof(client_addr);
    int client_socket = accept(socket_id, (struct sockaddr *)&client_addr, &length);

    if(client_socket < 0) {
        printf("Error accepting client connection\n");
        printf("Errno: %d\n", errno);
    }

    return client_socket;
}

void* process_client_request(void* input) { //void pointer return so cant do exit str method to exit
    struct thread_data *data;
    data = (struct thread_data *) input;

    printf("Thread: %d, data pointer %p\n", gettid(), data);

    printf("Data client socket beggining: %d\n", data->client_socket);

    int client_socket = data -> client_socket;
    int server_socket = data -> server_socket;

    char buff[MAX_MSG_LEN];
    memset(buff, 0, sizeof(buff));
    int rc = read(client_socket, buff, MAX_MSG_LEN);
    if(rc < 0) {
        printf("Error reading from client");
    }

    char method[8], operation[64], version[16];

    // Assuming buff contains the request string (like "GET /Exit HTTP/1.1")
    sscanf(buff, "%s %s %s", method, operation, version);
    
    if(strcmp(operation, "/Exit") == 0) {
        printf("exit recieved\n");
        close(client_socket);
        close(server_socket);
        return NULL;
    }

    printf("From client: %s\n", operation);

    int num1;
    int num2;
    int summ;
    char html_reply[MAX_REPLY_LEN];

    if (sscanf(operation, "/add_%d_%d", &num1, &num2) == 2) {
        summ = num1 + num2;
        
        snprintf(html_reply, MAX_REPLY_LEN, "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html; charset=UTF-8\r\n"
          "Connection: keep-alive\r\n"
          "\r\n"
          "<!DOCTYPE html>"
          "<html><body>%d + %d = %d</body></html>", num1, num2, summ);
        
        write(client_socket, html_reply, strlen(html_reply));
        //read again to see if client send something else (it doesnt)
        // int rc = read(client_socket, buff, MAX_MSG_LEN);
        // if(rc < 0) {
        //     printf("Error reading from client 2");
        // }
        // printf("from client 2: %s\n", buff)
    } else {
        printf("Failed to parse the string.\n");
        snprintf(html_reply, MAX_REPLY_LEN, "HTTP/1.1 400 BAD REQUEST\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "\r\n"
        "<!DOCTYPE html>"
        "<html><body>Invalid Request</body></html>");

        write(client_socket, html_reply, strlen(html_reply));
    }

    sleep(10);

    printf("Data client socket end: %d\n", data->client_socket);

    if(close(client_socket) < 0) {
        printf("error closing client socket");
    }

    free(data);
} //break down this func in different routines


int main() {
   int socket_id = create_socket();

   int bind_val = bind_socket("127.0.0.1", 80, socket_id);

   if((listen(socket_id, 3)) < 0) {
    printf("error listening\n");
    printf("Errno: %d\n", errno);
   }

   while(1) {
    struct thread_data *td = malloc(sizeof(struct thread_data)); //if not use malloc this will be used by multiple threads. mallac keeps var alive after while loope finishe
    td->server_socket = socket_id;

    int client_socket = accept_conns(socket_id);

    if(errno == 9) { //when str "Exit" is recieved
        printf("Server socket closed or error with client socket\n");
        break;
    } 

    td->client_socket = client_socket;

    if(client_socket < 0) {
        printf("Error accepting client connection");
    }

    pthread_t thread1;

    pthread_create(&thread1, NULL, process_client_request, (void *) td);

    // if((process_client_request(client_socket, socket_id)) == -1 ) { //exit str
    //     break;
    // }
    
   }
   printf("exiting\n");

}

//make chat system
//pass in command line ip + port who to talk to
//one thread that reads and one the writes to other side
//