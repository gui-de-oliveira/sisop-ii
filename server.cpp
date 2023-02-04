#include <iostream>
#include <list>
#include <future>
#include <unistd.h>
#include <sstream>
#include <string.h>

#include "socket.h"

using namespace std;

void onConnect(int socket);

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

int clientCounter = 0;
void onConnect(int socket)
{
    int clientId = clientCounter++;
    std::cout << Color::blue << "New client connected. Id: " << clientId << Color::reset << std::endl;

    char buffer[MAX_BUFFER_SIZE];
    bool closeConnection = awaitMessage(&buffer, socket);

    if (closeConnection)
    {
        close(socket);
        std::cout << "Login failed for Client id " << clientId << std::endl;
    };

    char username[MAX_BUFFER_SIZE];
    strcpy(username, buffer);

    std::cout << "Client " << clientId << " logged in as " << username << std::endl;

    std::ostringstream clientNameStream;
    clientNameStream << Color::yellow << "[" << clientId << "]" << Color::reset;
    std::string clientName = clientNameStream.str();

    while (true)
    {
        bool closeConnection = awaitMessage(&buffer, socket);
        Message::Ok().send(socket);

        if (closeConnection)
        {
            std::cout << clientName << " ended connection" << std::endl;
            break;
        }

        Message message = Message::Parse(buffer);

        if (message.type == MessageType::UploadCommand)
        {
            std::cout << "Upload command from " << clientName << std::endl;

            while (true)
            {
                bool closeConnection = awaitMessage(&buffer, socket);
                Message::Ok().send(socket);

                if (closeConnection)
                    break;

                Message message = Message::Parse(buffer);

                if (message.type == MessageType::DataMessage)
                {
                    std::cout << "Upload package received from " << clientName << std::endl;
                    std::cout << clientName << ": " << buffer << std::endl;
                    continue;
                }

                if (message.type == MessageType::EndCommand)
                {
                    std::cout << "End upload received from " << clientName << std::endl;
                    break;
                }

                std::cout << "Unhandled message from " << clientName << ": " << buffer << std::endl;
            }
            continue;
        }

        std::cout << "Unhandled message from " << clientName << ": " << buffer << std::endl;
    }

    close(socket);
    std::cout << "Connection with " << clientName << " closed" << std::endl;
}