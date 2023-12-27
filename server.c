#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define PORT 8085
#define BACKLOG 5
#define BUFFER_SIZE 1024

volatile sig_atomic_t wasSigHup = 0;

void sigHupHandler(int r) 
{
    wasSigHup = 1;
}

int main() 
{
    int serverFD;
    int incomingSocketFD = 0; 
    struct sockaddr_in socketAddress; 
    int addressLength = sizeof(socketAddress);
    fd_set fds;
    sigset_t blockedMask, origMask;
    int maxFd;

    // Socket creating
    if ((serverFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    {
        perror("create error");
        exit(EXIT_FAILURE);
    }

    // Setting socket address parameters
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;
    socketAddress.sin_port = htons(PORT);

    // Socket binding to the address
    if (bind(serverFD, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) 
    {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    // Started socket listening
    if (listen(serverFD, BACKLOG) < 0) 
    {
        perror("listen error");
        exit(EXIT_FAILURE);
    }   

    printf("Server started on port %d \n\n", PORT);

    // Setting up signal blocking
    sigemptyset(&blockedMask);
    sigemptyset(&origMask);
    sigaddset(&blockedMask, SIGHUP);
    sigprocmask(SIG_BLOCK, &blockedMask, &origMask);

    // Signal handler registration
    struct sigaction sa;
    int sigactionFirst = sigaction(SIGHUP, NULL, &sa);
    sa.sa_handler = sigHupHandler;
    sa.sa_flags |= SA_RESTART;
    int sigactionSecond = sigaction(SIGHUP, &sa, NULL);

    while (1) 
    {
        FD_ZERO(&fds); 
        FD_SET(serverFD, &fds); 
        if (incomingSocketFD > 0) 
            FD_SET(incomingSocketFD, &fds); 
        if (incomingSocketFD > serverFD)
            maxFd = incomingSocketFD;
        else 
            maxFd = serverFD; 
        if (pselect(maxFd + 1, &fds, NULL, NULL, NULL, &origMask) != -1)
        {
            if (wasSigHup) 
            {
                printf("SIGHUP received.\n");
                wasSigHup = 0;
                continue;
            }   
        } else {
            if (errno != EINTR) 
            {
                perror("pselect error"); 
                exit(EXIT_FAILURE); 
            }
        }
        // Reading incoming bytes
        if (incomingSocketFD > 0 && FD_ISSET(incomingSocketFD, &fds)) 
        { 
            char buffer[BUFFER_SIZE] = { 0 };
            int readBytes = read(incomingSocketFD, buffer, BUFFER_SIZE);
                if (readBytes > 0) 
                { 
                    printf("Received data: %d bytes\n", readBytes);
                    printf("Received message from client: %s\n", buffer);
                    const char* response = "Hello from server!";
                    if (send(incomingSocketFD, response, strlen(response), 0) < 0) 
                        perror("send error");
                } 
                else 
                {
                    if (readBytes == 0) 
                    {
                        close(incomingSocketFD); 
                        incomingSocketFD = 0; 
                        printf("Connection closed\n\n");
                    } 
                    else 
                        perror("read error");  
                } 
            continue;
        }       
        // Check of incoming connections
        if (FD_ISSET(serverFD, &fds)) 
        {
            if ((incomingSocketFD = accept(serverFD, (struct sockaddr*)&socketAddress, (socklen_t*)&addressLength)) < 0) 
            {
                perror("accept error");
                exit(EXIT_FAILURE);
            }
            printf("New connection.\n");
        }     
    }
    close(serverFD);
    return 0;
}
