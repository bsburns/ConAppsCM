// Client side implementation of UDP client-server model

#include <bits/stdc++.h>
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

#define PORT     8080
#define MAXLINE  1024

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

int main() {
    int sockfd;
    char buffer[MAXLINE];
    const char* hello = "Hello from client";
    struct sockaddr_in servaddr;

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

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Fill server address info
    servaddr.sin_family = AF_INET;              // IPv4
    servaddr.sin_port = htons(PORT);          // Server port
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP

    socklen_t len = sizeof(servaddr);

    // Send message to server
    sendto(sockfd, hello, strlen(hello), MSG_CONFIRM,
        (const struct sockaddr*)&servaddr, sizeof(servaddr));
    printf("Hello message sent.\n");

    // Receive reply from server
    int n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL,
        (struct sockaddr*)&servaddr, &len);

    buffer[n] = '\0';   // Null terminate received data
    printf("Server: %s\n", buffer);

    // Close socket
    close(sockfd);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}