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
#include <pthread.h>

int   client = 0;
int   server = 0;
int   client_sock;
int   server_sock;
int   BUFFER_SIZE = 4096;
int   SERVER_PORT = 5060;
char* SERVER_IP   = "127.0.0.1";
int thread_alive = 0;

void HandleExit(int signal) {
    printf("\nSignal received. Closing sockets and exiting...\n");

    if(client_sock != 0) close(client_sock);
    if(server_sock != 0) close(server_sock);

    exit(0);
}

void* receiveMessages(void *sock)
{   
    thread_alive = 1;
    
    int socket = *(int*)sock;

    char buffer[BUFFER_SIZE];

    while (1)
    {
        if (recv(socket, buffer, BUFFER_SIZE, 0) <= 0) break;

        if(strcmp(buffer, "exit\n") == 0)
        {
            if(server == 0) printf("Chat ended by the server.\n");
            if(client == 0) printf("Chat ended by the client.\n");
            close(socket);
            break;
        }

        if (server == 0) printf(">> Server: %s", buffer);
        if (client == 0) printf(">> Client: %s", buffer);
        memset(&buffer, 0, sizeof(buffer));
    }

    thread_alive = 0;
    pthread_exit(NULL);
}

int run_server()
{
    int temp = 0;             // Setting a temporary variable to help check for errors throughout the program.

    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Setting the listening socket
    if (server_sock == -1)
    {
        printf("Error : Listen socket creation failed.\n"); // if the creation of the socket failed,
        return 0;                                           // print the error and exit main.
    }

    int reuse = 1;                                                                   
    temp = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)); // uses a previously chosen socketopt if there is one.
    if (temp < 0)
    {
        printf("Error : Failed to set congestion control algorithm to reno.\n");      // If the socket's reuse failed,
        return 0;                                                                     // print the error and exit main.
    }

    struct sockaddr_in server_add;                  // Using the imported struct of a socket address using the IPv4 module.
    memset(&server_add, 0, sizeof(server_add)); // Resetting it to default values.

    server_add.sin_family = AF_INET;                // Server address is type IPv4.
    server_add.sin_addr.s_addr = INADDR_ANY;        // Get any address that tries to connect.
    server_add.sin_port = htons(SERVER_PORT);       // Set the server port to the defined 'server_port'.

    temp = bind(server_sock, (struct sockaddr *)&server_add, sizeof(server_add)); // Bind the socket to a port and IP.
    if (temp == -1)
    {
        printf("Error: Binding failed.\n");  // If the binding failed,
        close(server_sock);                  // print the corresponding error, close the socket and exit main.
        return -1;
    }

    temp = listen(server_sock, 3);            // Start listening, and set the max queue for awaiting client to 3.
    if (temp == -1)
    {
        printf("Error: Listening failed.");              // If listen failed,
        close(server_sock);                               // print the corresponding error, close the socket and exit main.
        return -1;
    }

    // Accept and incoming connection
    struct sockaddr_in client_add;                   // Using the imported struct of a socket address using the IPv4 module.
    socklen_t client_add_len = sizeof(client_add);   // Setting the size of the address to match what's needed.

    while(1)
    {
        printf("Waiting for a connection...\n");
        
        memset(&client_add, 0, sizeof(client_add));                                       // Resetting address struct to default values.
        client_add_len = sizeof(client_add);                                                  // Setting the size of the address to match what's needed.
        
        client_sock = accept(server_sock, (struct sockaddr *)&client_add, &client_add_len); // Accepting the clients request to connect.
        if (client_sock == -1)
        {
            printf("Error: Accepting a client failed.");       // If accept failed,
            close(server_sock);                                // print an error, close the socket and exit main.
            return -1;
        }

        printf("Connected to client!\n");

        // Create receive thread for each client
        pthread_t receiver;
        if (pthread_create(&receiver, NULL, receiveMessages, (void*)&client_sock) != 0) 
        {
            perror("Thread creation failed");
            exit(1);
        }

        char buffer[BUFFER_SIZE];
        memset(&buffer, 0, sizeof(buffer));

        // Start sending messages to the client
        while (1) 
        {    
            fgets(buffer, BUFFER_SIZE, stdin);

            if(thread_alive == 0) break;

            printf(">> Server: %s", buffer);

            if(send(client_sock, buffer, strlen(buffer), 0) == -1) 
            {
                perror("Sending failed");
                close(client_sock);
                close(server_sock);
                exit(1);
            }
            
            if (strcmp(buffer, "exit\n") == 0) 
            {
                printf("Chat ended by the server.\n");
                close(client_sock);
                break;
            }
        }
    }

    // Close the server socket
    close(server_sock);

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

int run_client()
{
    int temp = 0;                                           // Setting a temporary variable to help check for errors throughout the program.

    int client_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   // Creating a socket using the IPv4 module.

    if (client_sock == -1) {
        printf("Error : Listen socket creation failed.\n"); // If the socket creation failed, print the error and exit main.   
        return -1;
    }

    struct sockaddr_in server_add;                       // Using the imported struct of a socket address using the IPv4 module.
    memset(&server_add, 0, sizeof(server_add));       // Resetting it to default values.

    server_add.sin_family = AF_INET;
    server_add.sin_port = htons(SERVER_PORT);                                              // Changing little endian to big endian.
    temp = inet_pton(AF_INET, (const char *)SERVER_IP, &server_add.sin_addr);          // Converting the IP address from string to binary.

    if (temp <= 0) {
        printf("Error: inet_pton() failed.\n");                                               // If the conversion failed, print the error and exit main.
        return -1;
    }


    // Make a connection to the server with socket SendingSocket.

    int connectResult = connect(client_sock, (struct sockaddr *)&server_add, sizeof(server_add)); 
    if (connectResult == -1) {
        printf("Error: Connecting to server failed.\n");   
        close(client_sock);
        return -1;
    }

    printf("Connected to server!\n");

    char buffer[BUFFER_SIZE];
    memset(&buffer, 0, sizeof(buffer));

    pthread_t receiver;
    if (pthread_create(&receiver, NULL, receiveMessages, (void*)&client_sock ) != 0) 
    {
        perror("Thread creation failed");
        exit(1);
    }

    while(1)
    {
        fgets(buffer, BUFFER_SIZE, stdin);
        printf(">> Client: %s", buffer);

        if(send(client_sock, buffer, strlen(buffer), 0) == -1)
        {
            perror("Sending failed");
            close(client_sock);
            exit(1);
        }

        if (strcmp(buffer, "exit\n") == 0) 
        { 
            printf("Chat ended by the server.\n"); 
            break; 
        }
    }

    printf("Chat ended.\n");
    close(client_sock);

    return 0;
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
        else if (!strcmp(argv[i], "IP") || !strcmp(argv[i], "ip")) ;
        else if (!strcmp(argv[i], "PORT") || !strcmp(argv[i], "port")) ;
        else
        {
            printf("Error: Flag is not recognised. Exiting Program. \n");
            return 1;
        }
    }

    if(server == 1)
    {
        printf("Running server...\n");
        run_server();
    }
    else if(client == 1) 
    {
        printf("Running client...\n"); 
        run_client();
    }
    else
    {
        printf("Error: Server/Client flag missing. Exiting Program. \n");
        return 1;
    }

}