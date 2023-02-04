#include <iostream>
#include <list>
#include <future>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string.h>
#include <sys/stat.h>

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

    string username = buffer;
    string folder = "out/" + username + "/";
    mkdir(folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

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

            std::fstream file;
            file.open(folder + message.filename, ios::out);

            while (true)
            {
                bool closeConnection = awaitMessage(&buffer, socket);
                Message::Ok().send(socket);

                if (closeConnection)
                    break;

                Message message = Message::Parse(buffer);

                if (message.type == MessageType::DataMessage)
                {
                    std::cout << "Data from " << clientName << ": " << message.data << std::endl;
                    file << message.data;
                    continue;
                }

                if (message.type == MessageType::EndCommand)
                {
                    std::cout << "End from " << clientName << std::endl;
                    break;
                }

                std::cout << "Unhandled message from " << clientName << ": " << buffer << std::endl;
            }
            file.close();
            continue;
        }

        std::cout << "Unhandled message from " << clientName << ": " << buffer << std::endl;
    }

    close(socket);
    std::cout << "Connection with " << clientName << " closed" << std::endl;
}