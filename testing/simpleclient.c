#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
//headers explained in simpleserver.c
#include <netdb.h> // For gethostbyname

int main(){
    int client_fd;
    client_fd=socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd<0){
        perror("client socket() failed");
        exit(1);
    }
    printf("1. Client socket created succesfully");

    int port=9000;
    struct sockaddr_in server_addr;
    struct hostent *server;
    //struct to hold info about the server

    server=gethostbyname("localhost");
    if(server==NULL){
        fprintf(stderr, "No such host as localhost, error");
        exit(1);
    }

    // 2. setting up the server_addr struct (same like in the server)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port);

    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    // 3. Copy the server's IP address into the struct
    //    We use h_addr_list[0] - the first address in the list

    if(connect(client_fd,(struct sockaddr *) &server_addr, sizeof(server_addr))<0){
        perror("connect() failed");
        exit(1);
    }
    // 4. Connect to the server!
    // This call blocks until the connection is made or fails.

    printf("2. Connected to server yayy!!");

    char buffer[1024];
    char* message="Hello from client\n";
    write(client_fd, message, strlen(message));
    printf("Sent message: %s", message);
    
    memset(buffer, 0, 1024);
    read(client_fd, buffer, 1023);

    printf("Server echoed: %s",buffer);
    // close(client_fd);
    return 0;

}