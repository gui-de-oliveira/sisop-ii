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
#include <functional>
#include <future>
#include <unistd.h>
#include <cstdlib>
#include <sstream>
#include <list>

using namespace std;
#define MAX_BUFFER_SIZE 1500

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
        cerr << "Error establishing the server socket" << endl;
        exit(0);
    }

    int bindStatus = bind(
        serverConnection,
        (struct sockaddr *)&servAddr,
        sizeof(servAddr));

    if (bindStatus < 0)
    {
        cerr << "Error binding socket to local address" << endl;
        exit(0);
    }

    cout << "Server started!" << endl;

    return serverConnection;
}

int awaitConnection(int serverSocketDescriptor)
{
    int maxNumberOfConnections = 99;
    int result = listen(serverSocketDescriptor, maxNumberOfConnections);

    if (result == -1)
    {
        cerr << "Error listening to socket" << endl;
        exit(0);
    }

    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);

    int clientSocketDescriptor = accept(serverSocketDescriptor, (sockaddr *)&newSockAddr, &newSockAddrSize);
    if (clientSocketDescriptor < 0)
    {
        cerr << "Error accepting request from client!" << endl;
        exit(1);
    }

    return clientSocketDescriptor;
}

void clearBuffer(char (*buffer)[MAX_BUFFER_SIZE])
{
    memset(buffer, 0, sizeof(*buffer));
}

bool awaitMessage(char (*buffer)[MAX_BUFFER_SIZE], int socketDescriptor)
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

void sendMessage(int socket, string message)
{
    char buffer[MAX_BUFFER_SIZE];
    clearBuffer(&buffer);
    strcpy(buffer, message.c_str());
    send(socket, (char *)&buffer, strlen(buffer), 0);
    clearBuffer(&buffer);
}

int clientCounter = 0;
void onConnect(int socket)
{
    int clientId = clientCounter++;
    cout << "New client connected. Id: " << clientId << endl;

    char buffer[MAX_BUFFER_SIZE];
    bool closeConnection = awaitMessage(&buffer, socket);

    if (closeConnection)
    {
        close(socket);
        cout << "Login failed for Client id " << clientId << endl;
    };

    char username[MAX_BUFFER_SIZE];
    strcpy(username, buffer);

    cout << "Client " << clientId << " logged in as " << username << endl;

    ostringstream clientNameStream;
    clientNameStream << "[Client " << clientId << " - " << username << "]";
    string clientName = clientNameStream.str();

    while (true)
    {
        bool closeConnection = awaitMessage(&buffer, socket);

        if (closeConnection)
        {
            cout << clientName << " ended connection" << endl;
            break;
        }

        cout << clientName << ": " << buffer << endl;
        sendMessage(socket, "OK!");
    }

    close(socket);
    cout << "Connection with " << clientName << " closed" << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Expected usage: ./server <port-number>" << endl;
        exit(-1);
    }

    int port = atoi(argv[1]);

    int serverSocketDescriptor = startServer(port);

    list<future<void> *> runningExecutions;
    while (true)
    {
        int clientSocketDescriptor = awaitConnection(serverSocketDescriptor);

        future<void> *connectionExecution = (future<void> *)malloc(sizeof(future<void>));

        *connectionExecution = async(launch::async, onConnect, clientSocketDescriptor);

        runningExecutions.push_back(connectionExecution);
    }

    return 0;
}