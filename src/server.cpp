#include <iostream>
#include <list>
#include <future>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <functional>
#include <map>
#include <time.h>

#include "libs/message.h"
#include "libs/fileManager.h"

using namespace std;

void onConnect(Session, ThreadSafeQueue<FileAction> *);

class Singleton
{
protected:
    AsyncRunner *runner;

public:
    ThreadSafeQueue<FileAction> *fileQueue;
    FilesManager *fileManager;

    Singleton(ThreadSafeQueue<FileAction> *_fileQueue, AsyncRunner *_runner, FilesManager *_fileManager)
    {
        fileQueue = _fileQueue;
        runner = _runner;
        fileManager = _fileManager;
    }

    void start(Session session)
    {
        runner->queue(
            async(launch::async, onConnect, session, fileQueue));
    }
};

void processQueue(Singleton *singleton)
{
    while (true)
    {
        FileAction fileAction = singleton->fileQueue->pop();

        string username = fileAction.session.username;

        UserFiles *userFiles = singleton->fileManager->getFiles(username);
        FileState lastFileState = userFiles->get(fileAction.filename);

        Callback onComplete = [fileAction, singleton]()
        {
            singleton->start(fileAction.session);
        };

        userFiles->fileStatesByFilename[fileAction.filename] = getNextState(lastFileState, fileAction, onComplete);
    }
}

std::future<void> startQueueProcessor(Singleton *singleton)
{
    std::future<void> queueProcessor = async(launch::async, processQueue, singleton);
    return queueProcessor;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Expected usage: ./server <port-number>" << endl;
        exit(-1);
    }

    int port = atoi(argv[1]);

    int serverSocket = startServer(port);

    AsyncRunner runner;
    ThreadSafeQueue<FileAction> queue;
    FilesManager fileManager;
    Singleton singleton(&queue, &runner, &fileManager);

    auto queueProcessor = async(launch::async, processQueue, &singleton);

    int clientCounter = 0;
    while (true)
    {
        int clientSocket = awaitConnection(serverSocket);
        int clientId = clientCounter++;

        std::cout << Color::blue << "New client connected. Id: " << clientId << Color::reset << std::endl;

        char buffer[MAX_BUFFER_SIZE];
        bool closeConnection = listenPacket(&buffer, clientSocket);
        Message::Ok().send(clientSocket);

        if (closeConnection)
        {
            close(clientSocket);
            std::cout << "Login failed for Client id " << clientId << std::endl;
            continue;
        };

        string username = buffer;
        string folder = "out/" + username + "/";
        mkdir(folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        std::cout << "Client " << clientId << " logged in as " << username << std::endl;
        Session session(clientId, clientSocket, username);

        singleton.start(session);
    }

    return 0;
}

void onConnect(Session session, ThreadSafeQueue<FileAction> *queue)
{
    std::ostringstream clientNameStream;
    clientNameStream << Color::yellow << "[" << session.clientId << "]" << Color::reset;
    std::string clientName = clientNameStream.str();

    while (true)
    {
        char buffer[MAX_BUFFER_SIZE];
        bool closeConnection = listenPacket(&buffer, session.socket);

        if (closeConnection)
        {
            std::cout << clientName << " ended connection" << std::endl;
            break;
        }

        Message message = Message::Parse(buffer);

        if (message.type == MessageType::UploadCommand)
        {
            queue->queue(FileAction(session, message.filename, FileActionType::Upload, message.timestamp));
            return;
        }

        if (message.type == MessageType::DownloadCommand)
        {
            queue->queue(FileAction(session, message.filename, FileActionType::Read, message.timestamp));
            return;
        }

        if (message.type == MessageType::Ok || message.type == MessageType::EndCommand || message.type == MessageType::DataMessage)
        {
            std::cout << "[ignored] " << message.type << " from " << clientName << std::endl;
            continue;
        }

        std::cout << "Unhandled message from " << clientName << ": " << buffer << std::endl;
    }

    close(session.socket);
    std::cout << "Connection with " << clientName << " closed" << std::endl;
}