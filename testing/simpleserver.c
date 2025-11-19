#include <stdio.h>
//for printf and perror

#include <stdlib.h>
//for exit

#include <string.h>
//for bzero or memset

#include <unistd.h>
//for read write close all

#include <sys/socket.h>
//for main socket functions

#include <netinet/in.h>
//for the struct sockaddr_in and htons()/htonl() macros.

#include <pthread.h>
//for threading

// This is the function that each thread will run
void *handle_client(void *arg){
    // 1. Get our client file descriptor back by casting arg to (int*) then dereferencing to get the integer file descriptor.
    int client_fd = *( (int*)arg );
    free(arg); // We've copied the value, so we free the memory
    // free(arg) releases the heap memory you allocated in the main thread to avoid leaks.

    char buffer[1024]; // A buffer to hold the client's message
    int bytes_read;

    // Clear the buffer
    memset(buffer, 0, 1024);

    // read() will block and wait for the client to send data
    bytes_read = read(client_fd, buffer, 1023); // 1023 to leave room for '\0'

    if (bytes_read < 0) {
        perror("read() failed");
    } else if (bytes_read == 0) {
        printf("Client disconnected before sending a message.\n");
    } else {
        // We got a message!
        printf("Client said: %s", buffer); // printf the message

        // Now, echo it back
        write(client_fd, buffer, bytes_read);
        printf("Echoed message back to client.\n");
    }

    // Now we're done with this client
    close(client_fd);
    printf("Client disconnected. Waiting for next client...\n");
    return NULL;
}

int main(int argc, char*argv[]){
    printf("Starting server...\n");
    
    int server_fd; //server file descriptor
    server_fd=socket(AF_INET,SOCK_STREAM,0);
    // AF_INET = Use IPv4 (the standard internet protocol)
    // SOCK_STREAM = Use TCP (a reliable, streaming connection, not UDP)
    // 0 = Use the default protocol (which is TCP for SOCK_STREAM)
    
    // ALWAYS check for errors.
    // A negative return value means the function failed.
    if(server_fd<0){
        // perror prints your message ("socket() failed")
        // AND the specific system error (like "Permission denied").
        perror("socket() function failed");
        exit(1);
    }
    printf("1. Socket created successfully (fd=%d) \n",server_fd);

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }
    //When your server exits (or crashes), the operating system keeps port 9000 in a "TIME_WAIT" state for about 30-60 seconds. It's "reserving" the port just in case any last-second data packets arrive.
    //The Solution: SO_REUSEADDR, We need to tell the OS, "Hey, I'm the new server, and I give you permission to reuse that address right now.", We do this with a function called setsockopt().

    struct sockaddr_in server_addr;
    //Different kinds of networks (IPv4, IPv6, local domain sockets, etc.) each have their own struct to represent an address.
    //AF_INET and struct sockaddr_in for IPv4, AF_INET6 and struct sockaddr_in6 for IPv6, etc.


    int port=9000;
    //we can also take this from command-line using if (argc > 1) port = atoi(argv[1]);

    // Clear the entire struct to make sure it's clean
    // We want all fields clean before assigning values.
    // Prevents accidental garbage values from stack memory.
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    // Sets the address family to IPv4 (must match what we used in the socket else bind() will fail with EINVAL (invalid argument))

    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Sets the IP address to INADDR_ANY which means "listen on any available IP address" This is what we want for a server.
    //INADDR_ANY is a macro meaning “all network interfaces”.In binary, it’s 0.0.0.0. 

    server_addr.sin_port = htons(port);
    // Sets the port number, converting from host byte order to network byte order
    // htons() = "Host to Network Short"
    // All TCP/IP headers use network byte order (big-endian).
    // If you skip this conversion, other computers would interpret the bytes backwards — e.g.,
    // 9000 (0x2328) might become 0x2823 = 10275, i.e., completely wrong port.

    //bind() is the system call that tells the operating system, "I want the socket server_fd to be the one responsible for port 9000 (we have assigned server_addr's sin_port as port)."
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        // We cast our 'struct sockaddr_in' (which is internet-specific)
        // to the generic 'struct sockaddr' that bind() expects.
        perror("bind() failed");
        //checking for errors. A common error here is "Address already in use," which means another program (or your old, crashed server) is still holding onto port 9000.
        exit(1);
    }
    printf("2. Socket bound to port %d\n", port);

    // listen(server_fd, 5) tells the OS: "If I'm busy in my accept() loop handling a new connection, you can hold up to 5 other new connections in a 'pending' queue. If a 6th connection arrives while the queue is full, just reject it."
    // Since your accept() loop is so fast (it just calls pthread_create and loops), the backlog is rarely hit, but it's important to have. 5 or 20 is a fine number for this.
    if (listen(server_fd, 5) < 0) {
        perror("listen() failed");
        exit(1);
    }
    
    printf("3. Server is listening on port %d...\n", port);

    int client_fd; // This will be the NEW file descriptor for the client
    struct sockaddr_in client_addr; // This will hold the CLIENT's address info
    socklen_t client_len = sizeof(client_addr);
    printf("Waiting for a client to connect...\n");
    while (1) {
        // now accept() blocks the program and waits for a connection
        client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_len);
        
        if (client_fd < 0) {
            perror("accept() failed");
            continue; // Go back to the start of the loop and wait again
        }

        // printf("4. Client connected successfully! Waiting for a message...\n");
        printf("4. Client connected! Handing off to a new thread...\n");

        pthread_t tid;

        int *new_sock=malloc(sizeof(int));
        *new_sock=client_fd;
        // We can't just pass &client_fd to the thread. Because the main loop will immediately loop back, accept a new client, and change the value of client_fd. The first thread would suddenly have its file descriptor changed! By malloc-ing new memory, we give each thread its own private copy of the file descriptor.
    // Why not pass &client_fd? If you passed the address of the stack variable client_fd from the main thread, that memory could change when the main loop accepts the next connection; threads would race and get wrong FDs. Allocating per-thread memory avoids that race.

        if(pthread_create(&tid, NULL, handle_client, (void*)new_sock)!=0){
            perror("pthread_create() failed");
        }
        // Create a new thread:
        // 1. &tid: Store the thread's ID here
        // 2. NULL: Use default thread attributes
        // 3. handle_client: The function the new thread should run
        // 4. new_sock: The argument to pass to that function
        // We need to pass the client_fd to the thread, but pthread_create only accepts a (void*) so we cast.

    }
    close(server_fd);
    return 0;
}