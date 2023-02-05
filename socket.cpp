#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>

#include "socket.h"

std::string Color::red = "\033[31m";
std::string Color::green = "\033[32m";
std::string Color::yellow = "\033[33m";
std::string Color::blue = "\033[34m";
std::string Color::reset = "\033[0m";

void clearBuffer(char (*buffer)[MAX_BUFFER_SIZE])
{
    memset(buffer, 0, sizeof(*buffer));
}

bool listenPacket(char (*buffer)[MAX_BUFFER_SIZE], int socketDescriptor)
{
    clearBuffer(buffer);
    int bytesRead = recv(socketDescriptor, (char *)buffer, sizeof(*buffer), 0);

    bool errorOnRead = bytesRead == -1;
    bool isResponseEmpty = bytesRead == 0;
    if (errorOnRead || isResponseEmpty)
    {
        return true;
    }

    return false;
}

void sendPacket(int socket, std::string message)
{
    char buffer[MAX_BUFFER_SIZE];
    clearBuffer(&buffer);
    strcpy(buffer, message.c_str());
    send(socket, (char *)&buffer, strlen(buffer), 0);
    clearBuffer(&buffer);
}

void sendCustomPacket(int socket)
{
    std::string data;
    getline(std::cin, data);
    sendPacket(socket, data);
}

void awaitOk(int socket)
{
    char buffer[MAX_BUFFER_SIZE];
    listenPacket(&buffer, socket);
}

// = CLIENT METHODS ========================================================================

int connectToServer(char *address, int port)
{
    sockaddr_in sendSockAddr;
    bzero((char *)&sendSockAddr, sizeof(sendSockAddr));

    sendSockAddr.sin_family = AF_INET;
    sendSockAddr.sin_port = htons(port);

    struct hostent *host = gethostbyname(address);
    sendSockAddr.sin_addr.s_addr =
        inet_addr(inet_ntoa(*(struct in_addr *)*host->h_addr_list));

    int connection = socket(AF_INET, SOCK_STREAM, 0);

    int status = connect(
        connection,
        (sockaddr *)&sendSockAddr,
        sizeof(sendSockAddr));

    if (status < 0)
    {
        std::cout << "Error connecting to server!" << std::endl;
        exit(-1);
    }

    return connection;
}

// = SERVER METHODS ========================================================================

int startServer(int port)
{
    sockaddr_in servAddr;
    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    int serverConnection = socket(AF_INET, SOCK_STREAM, 0);
    if (serverConnection < 0)
    {
        std::cerr << "Error establishing the server socket" << std::endl;
        exit(0);
    }

    int bindStatus = bind(
        serverConnection,
        (struct sockaddr *)&servAddr,
        sizeof(servAddr));

    if (bindStatus < 0)
    {
        std::cerr << "Error binding socket to local address" << std::endl;
        exit(0);
    }

    std::cout << "Server started!" << std::endl;

    return serverConnection;
}

int awaitConnection(int serverSocketDescriptor)
{
    int maxNumberOfConnections = 99;
    int result = listen(serverSocketDescriptor, maxNumberOfConnections);

    if (result == -1)
    {
        std::cerr << "Error listening to socket" << std::endl;
        exit(0);
    }

    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);

    int clientSocketDescriptor = accept(serverSocketDescriptor, (sockaddr *)&newSockAddr, &newSockAddrSize);
    if (clientSocketDescriptor < 0)
    {
        std::cerr << "Error accepting request from client!" << std::endl;
        exit(1);
    }

    return clientSocketDescriptor;
}
