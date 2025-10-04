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

int bind_socket(int socket_id, struct sockaddr_in bind_args) {
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

int make_connect(int socket_id, struct sockaddr_in addr){
    int conn = connect(socket_id, (struct sockaddr*)&addr, sizeof(addr));

    if (conn < 0) {
        perror("Connection to server failed\n");
    }

    return conn;
}

int read_msg(int sock, char* buff) {
    int rc = read(sock, buff, MAX_MSG_LEN - 1);// have start and end byte to see when message ends
    if(rc < 0) {
        printf("Error reading\n");
    } else {
        buff[rc] = '\0';
    }

    return rc;
}

int write_msg(int sock, char* buff) {
    int wc = write(sock, buff, strlen(buff));

    if(wc < 0) {
        printf("Error writing\n");
    }

    return wc;
}

int check_buff(char* buff) {
    size_t len = strlen(buff);

    // Check if the read line contains '\n' => means user typed less than max len
    if (len > 0 && buff[len - 1] == '\n') {
        buff[len - 1] = '\0';  
        len -= 1; // Adjust length to exclude the newline

        return 1;
    } else {
        // No '\n' => the user typed at least MAX_MSG_LEN+1 chars before hitting Enter:
        // We must flush the rest of the line from stdin.
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            
        }
        printf("Error: input exceeds %d characters. Please try again.\n", MAX_MSG_LEN);
        
        return -1;
    }
}

void* recv_msg(void* input) {
    int* sock_id;
    sock_id = (int*) input;

    // printf("Thread: %d, data pointer %p\n", gettid(), input_data);

    char buff[MAX_MSG_LEN];
    memset(buff, 0, sizeof(buff));
    
    while(1) {
        int rc = read_msg(*sock_id, buff);
        // printf("rc: %i\n", rc);
        if(rc == 0) {
            printf("Connection has closed\n");
            return NULL;
        }

        printf("Message Received: : %s\n", buff);  
    }
}

void* send_msg(void* input) {
    struct thread_data *input_data;
    input_data = (struct thread_data *) input;

    // printf("Thread: %d, data pointer %p\n", gettid(), input_data);

    int client_socket = input_data -> client_socket;
    int server_socket = input_data -> server_socket;

    char buff[MAX_MSG_LEN];
    memset(buff, 0, sizeof(buff));

    while(1) {
        if (fgets(buff, sizeof(buff), stdin) == NULL) { //send 30 if more than 30, then send remaining message
            // EOF or read error
            printf("\nEnd of input. Exiting.\n");
            break;
        }

        if(check_buff(buff) < 0) {
            continue;
        }

        if(strcmp(buff, "Exit") == 0) {
            printf("Exiting\n");
            
            if(server_socket) {
                close(server_socket);
                printf("Closed Server Socket\n");
                return NULL;
            }

            close(client_socket);
            printf("Closed client socket\n");

            return NULL;
        }

        write_msg(client_socket, buff);

        memset(buff, 0, sizeof(buff));
    }
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s [server|client] <ip> <port>\n", argv[0]);
        return 1;
    }

    char* mode = argv[1];
    char* ip = argv[2];
    int port = atoi(argv[3]);

    int socket_id = create_socket();

    struct sockaddr_in bind_args;
    memset(&bind_args, 0, sizeof(bind_args));

    bind_args.sin_family = AF_INET;
    bind_args.sin_addr.s_addr = inet_addr(ip);
    bind_args.sin_port = htons(port);

    struct thread_data *td = malloc(sizeof(struct thread_data)); 

    if(strcmp(mode, "server") == 0) {
        bind_socket(socket_id, bind_args);

        if((listen(socket_id, 3)) < 0) { //stop program here if error
            printf("error listening\n");
            printf("Errno: %d\n", errno);
        }

        int client_socket = accept_conns(socket_id);

        td->client_socket = client_socket;
        td->server_socket = socket_id;

        if(client_socket > 0) {
            printf("Connected\n");
        }
    }

    if(strcmp(mode, "client") == 0) {
        if(make_connect(socket_id, bind_args) >= 0){
            printf("Connected\n");
        }

        

        td->client_socket = socket_id;
    }

    pthread_t read_thread;
    pthread_t write_thread;

    pthread_create(&write_thread, NULL, send_msg, (void *) td);
    pthread_create(&read_thread, NULL, recv_msg, (void *) &td->client_socket);

    pthread_join(write_thread, NULL);
    pthread_join(read_thread, NULL);

    printf("exiting\n");

}

//Exit str doesnt end other connection
//
//protocal for sending msg, STX len maybe checksum (no need for ETX bc of len)
//if len is < 255 send one byte as its val