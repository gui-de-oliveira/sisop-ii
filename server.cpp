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
#include <list>

using namespace std;

int startServer(int port)
{
    sockaddr_in servAddr;
    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    // open stream oriented socket with internet address
    // also keep track of the socket descriptor
    int serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSd < 0)
    {
        cerr << "Error establishing the server socket" << endl;
        exit(0);
    }

    // bind the socket to its local address
    int bindStatus = bind(
        serverSd,
        (struct sockaddr *)&servAddr,
        sizeof(servAddr));

    if (bindStatus < 0)
    {
        cerr << "Error binding socket to local address" << endl;
        exit(0);
    }

    return serverSd;
}

void onConnection(int serverSd, function<void(int, int)> onConnect)
{
    list<future<void> *> a;

    while (true)
    {
        cout << "Waiting for a client to connect..." << endl;
        int maxNumberOfConnections = 99;
        int result = listen(serverSd, maxNumberOfConnections);

        if (result == -1)
        {
            cerr << "Error listening to socket" << endl;
            exit(0);
        }

        // receive a request from client using accept
        // we need a new address to connect with the client
        sockaddr_in newSockAddr;
        socklen_t newSockAddrSize = sizeof(newSockAddr);

        // accept, create a new socket descriptor to
        // handle the new connection with client
        int connectionSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize);
        if (connectionSd < 0)
        {
            cerr << "Error accepting request from client!" << endl;
            exit(1);
        }
        future<void> *connectionExecution = (future<void> *)malloc(sizeof(future<void>));
        *connectionExecution = async(launch::async, onConnect, serverSd, connectionSd);
        a.push_back(connectionExecution);
    }
}

int clientCounter = 0;
void onConnect(int serverSd, int connectionSd)
{
    int clientId = clientCounter++;
    cout << "Connected with client " << clientId << endl;

    char buffer[1500];

    while (true)
    {
        memset(&buffer, 0, sizeof(buffer));

        recv(connectionSd, (char *)&buffer, sizeof(buffer), 0);

        if (!strcmp(buffer, "exit"))
        {
            cout << "Client " << clientId << " has quit the session" << endl;
            close(connectionSd);
            return;
        }

        cout << "Client " << clientId << ": " << buffer << endl;

        string data = "PING";

        memset(&buffer, 0, sizeof(buffer));
        strcpy(buffer, data.c_str());

        sleep(1);
        send(connectionSd, (char *)&buffer, strlen(buffer), 0);
    }

    close(connectionSd);

    cout << "Connection closed..." << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: ./server [port number]" << endl;
        exit(0);
    }

    int port = atoi(argv[1]);

    int serverSd = startServer(port);
    onConnection(serverSd, onConnect);

    return 0;
}