#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <fcntl.h>

int   client = 0;
int   server = 0;
int   testing = 0;
int   client_sock;
int   server_sock;
int   BUFFER_SIZE = 4096;
char* SERVER_PORT;
char* SERVER_IP;

void HandleExit(int signal) 
{
    printf("\nSignal received. Closing sockets and exiting...\n");

    if(client_sock > 0) close(client_sock);
    if(server_sock > 0) close(server_sock);

    exit(0);
}

void activate_poll(struct pollfd fds[2], char* buffer)
{     
    while(1)
    {
        bzero(buffer, BUFFER_SIZE);
        fflush(stdin);

        int poller = poll(fds, 2, 100);
        if(poller < 0)
        {
            printf("Error: Polling failed.\n");
            if(client_sock > 0) close(client_sock);
            if(server_sock > 0) close(server_sock);
            exit(1);
        }

        // If our poller caught an input from the serve'rs user, we read it and send it to the client.
        if(fds[0].revents & POLLIN)
        {            
            printf("User input detected.\n");
            
            int bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE);
            if(bytes_read < 0)
            {
                printf("%d", bytes_read);
                printf("Error: Reading from user failed.\n");
                if(client_sock > 0) close(client_sock);
                if(server_sock > 0) close(server_sock);
                exit(1);
            }
            buffer[bytes_read] = '\0';

            printf("Message: %s", buffer);

            int bytes_sent = send(client_sock, buffer, bytes_read, 0);
            if(bytes_sent < 0)
            {
                printf("%d", bytes_sent);
                printf("Error: Sending failed.\n");
                if(client_sock > 0) close(client_sock);
                if(server_sock > 0) close(server_sock);
                exit(1);
            }

            printf("Sent %d bytes to client.\n", bytes_sent);

            if(!strcmp(buffer, "exit\n"))
            {
                printf("Exiting...\n");
                if(client_sock > 0) close(client_sock);
                if(server_sock > 0) close(server_sock);
                exit(0);
            }           
        }

        // If the client has sent something, we read it and print it to the user.
        if(fds[1].revents & POLLIN)
        {
            printf("Socket action detected.\n");

            int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if(bytes_received < 0)
            {
                printf("%d", bytes_received);
                printf("Error: Receiving from client failed.\n");
                if(client_sock > 0) close(client_sock);
                if(server_sock > 0) close(server_sock);
                exit(1);
            }
            buffer[bytes_received] = '\0';

            if(server == 1) printf(">> Client: %s", buffer);
            if(client == 1) printf(">> Server: %s", buffer);
            
            // If the client sent an exit message, we close the socket and continue to wait for a new connection.
            if(!strcmp(buffer, "exit\n"))
            {
                printf("Exiting...\n");
                close(client_sock);
                memset(buffer, 0, bytes_received);
                
                break;
            }
        }
    }
}

void run_server()
{
    int temp;
    
    // Creating the socket.
    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    if (server_sock < 0)
    {
        printf("Error : Listen socket creation failed.\n");
        exit(1);
    }

    // Using a previously chosen socketopt if there is one.
    int reuse = 1;                                                                   
    temp = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
    if (temp < 0) 
    {
        printf("Error : Failed to set congestion control algorithm to reno.\n");
        exit(1);                          
    }

    // Creating the server address variable.
    struct sockaddr_in server_add;                  
    memset(&server_add, 0, sizeof(server_add)); 
    server_add.sin_family = AF_INET;                     // Server address is type IPv4.
    server_add.sin_addr.s_addr = INADDR_ANY; // Get any address that tries to connect.
    server_add.sin_port = htons(atoi(SERVER_PORT));            // Set the server port to the defined 'SERVER_PORT'.

    // Binding the socket to a port and IP.
    temp = bind(server_sock, (struct sockaddr *)&server_add, sizeof(server_add)); // Bind the socket to a port and IP.
    if (temp < 0)
    {
        printf("Error: Binding failed.\n");  
        close(server_sock);
        exit(1);
    }

    // Start listening, and set the max queue for awaiting client to 3.
    temp = listen(server_sock, 1);
    if (temp < 0)            
    {
        printf("Error: Listening failed.");
        close(server_sock);
        exit(1);
    }
  
    // Creating the client address variable.
    struct sockaddr_in client_add;                   
    socklen_t client_add_len = sizeof(client_add); 
    memset(&client_add, 0, sizeof(client_add));

    // Creating a pollfd struct that will be used to check wether incoming data is from the client or from the server's user.
    struct pollfd fds[2] =
    {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = client_sock,  .events = POLLIN }
    };

    // Creating the buffer for the communication.
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); 

    while(1)
    {   
        // Server waits for a connection.
        printf("Waiting for a connection...\n");
        
        client_sock = accept(server_sock, (struct sockaddr *)&client_add, &client_add_len);
        if (client_sock < 0)
        {
            printf("Error: Accepting failed.\n");
            close(server_sock);
            exit(1);
        }

        printf("Connected to client!\n");

        // If we reach here, we have a connection.
        activate_poll(fds, buffer);
    }

    // If we reach here, then we close the server socket and exit the program.
    close(server_sock);
}

void run_client()
{
    int temp;
    
    client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   // Creating a socket using the IPv4 module.
    if (client_sock == -1) 
    {
        printf("Error : Listen socket creation failed.\n"); // If the socket creation failed, print the error and exit main.   
        exit(1);
    }

    struct sockaddr_in server_add;                       // Using the imported struct of a socket address using the IPv4 module.
    memset(&server_add, 0, sizeof(server_add));          // Resetting it to default values.

    server_add.sin_family = AF_INET;
    server_add.sin_port = htons(atoi(SERVER_PORT));                                              // Changing little endian to big endian.
    
    temp = inet_pton(AF_INET, SERVER_IP, &server_add.sin_addr);
    if (temp < 0)                                                                       // Converting the IP address from string to binary.
    {
        printf("Error: inet_pton() failed.\n");                                               // If the conversion failed, print the error and exit main.
        exit(1);
    }

    temp = connect(client_sock, (struct sockaddr *)&server_add, sizeof(server_add)); 
    if(temp < 0)
    {
        printf("Error: Connecting to server failed.\n");   
        close(client_sock);
        exit(1);
    }

    printf("Connected to server!\n");

    // Creating a pollfd struct that will be used to check wether incoming data is from the client or from the server's user.
    struct pollfd fds[2] =
    {
        { .fd = STDIN_FILENO, .events = POLLIN },
        { .fd = client_sock,  .events = POLLIN }
    };

    char buffer[BUFFER_SIZE];
    memset(&buffer, 0, sizeof(buffer));

    activate_poll(fds, buffer);
}

int main(int argc, char *argv[])
{
    signal(SIGTSTP, HandleExit); // Helps preventing crashing when closing the socket later on.
    
    // We check if the number of arguments is correct.
    // If not, we print an error message and exit the program.
    if (argc < 3)
    {
        printf("Error: received less variables than expected. \n");
        printf("Please execute the file as following: ./cmp <file1> <file2> (-v) (-i)\n");
        return 1;
    }

    // We go over the flags and check if they are valid.
    // If they are, we set the corresponding variable to 1.
    // If not, we print an error message, and return 1.
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-c"))      client = 1;
        else if (!strcmp(argv[i], "-s")) server = 1;
        else if (i==2 && !strcmp(argv[1],"-c")) SERVER_IP = argv[i]; 
        else if (i==2 && !strcmp(argv[1],"-s")) SERVER_PORT = argv[i];
        else if (i==3 && !strcmp(argv[1],"-c")) SERVER_PORT = argv[i];
        else { printf("Error: Executed incorrectly. Exiting Program. \n"); return 1; }
    }

    if      (server == 1) { printf("Running server...\n"); run_server(); }
    else if (client == 1) { printf("Running client...\n"); run_client(); }
    else                  { printf("Error: Server/Client flag missing. Exiting Program. \n"); return 1; }

}