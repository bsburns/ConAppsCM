// Server side implementation of UDP client-server model 
#include <bits/stdc++.h> 
#include <stdlib.h> 
#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#endif
#include <string.h> 
#include <sys/types.h> 

#define PORT     8080 
#define MAXLINE 1024 

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

// Driver code 
int main() {
    int sockfd;
    char buffer[MAXLINE];
    const char* hello = "Hello from server";
    struct sockaddr_in servaddr, cliaddr;
#ifdef _WIN32 
    WSADATA wsaData;
    int result;

    // 1. Initialize Winsock version 2.2
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed with error: " << result << std::endl;
        return 1;
    }

    std::cout << "Winsock initialized: " << wsaData.szDescription << std::endl;
#endif
    // Creating socket file descriptor 
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information 
    servaddr.sin_family = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Bind the socket with the server address 
    if (bind(sockfd, (const struct sockaddr*)&servaddr,
        sizeof(servaddr)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    socklen_t len;
    int n;

    len = sizeof(cliaddr);  //len is value/result 

    n = recvfrom(sockfd, (char*)buffer, MAXLINE,
        MSG_WAITALL, (struct sockaddr*)&cliaddr,
        &len);
    buffer[n] = '\0';
    printf("Client : %s\n", buffer);
    sendto(sockfd, (const char*)hello, strlen(hello),
        MSG_CONFIRM, (const struct sockaddr*)&cliaddr,
        len);
    std::cout << "Hello message sent." << std::endl;

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}