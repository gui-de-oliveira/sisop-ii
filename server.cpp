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

#include "socket.h"

using namespace std;

class Session
{
public:
    int clientId;
    int socket;
    string username;

    Session(int _clientId, int _socket, string _username)
    {
        clientId = _clientId;
        socket = _socket;
        username = _username;
    }
};

enum FileActionType
{
    Upload,
    Read,
    Delete
};

class FileAction
{
public:
    Session session = Session(0, 0, "");
    string filename;
    FileActionType type;

    FileAction(Session _session,
               string _filename,
               FileActionType _type)
    {
        filename = _filename;
        session = _session;
        type = _type;
    }
};

template <typename T>
class ThreadSafeQueue
{
protected:
    list<T> queued;

public:
    void queue(T data)
    {
        queued.push_back(data);
    }

    T pop()
    {
        while (queued.empty())
        {
            sleep(1);
        }

        T data = queued.front();
        queued.pop_front();
        return data;
    }
};

void onConnect(Session, ThreadSafeQueue<FileAction> *);

class Piper
{
protected:
    ThreadSafeQueue<future<void> *> executions;
    ThreadSafeQueue<FileAction> *fileQueue;

public:
    Piper(ThreadSafeQueue<FileAction> *_fileQueue)
    {
        fileQueue = _fileQueue;
    }

    void start(Session session)
    {
        future<void> *execution = (future<void> *)malloc(sizeof(future<void>));
        auto exec = async(launch::async, onConnect, session, fileQueue);
        execution = &exec;
        executions.queue(execution);
    }
};

ThreadSafeQueue<future<void> *> temporary;

void processQueue(ThreadSafeQueue<FileAction> *, Piper *);
std::future<void> startQueueProcessor(ThreadSafeQueue<FileAction> *, Piper *);

std::future<void> startQueueProcessor(ThreadSafeQueue<FileAction> *queue, Piper *piper)
{
    std::future<void> queueProcessor = async(launch::async, processQueue, queue, piper);
    return queueProcessor;
}

void processQueue(ThreadSafeQueue<FileAction> *queue, Piper *piper)
{
    while (true)
    {
        FileAction fileAction = queue->pop();
        Message::Ok().send(fileAction.session.socket);
        auto run = [fileAction, piper]
        {
            std::fstream file;
            string filePath = "out/" + fileAction.session.username + "/" + fileAction.filename;
            file.open(filePath, ios::out);

            while (true)
            {
                char buffer[MAX_BUFFER_SIZE];
                bool closeConnection = awaitMessage(&buffer, fileAction.session.socket);
                Message::Ok().send(fileAction.session.socket);

                if (closeConnection)
                    break;

                Message message = Message::Parse(buffer);

                if (message.type == MessageType::DataMessage)
                {
                    std::cout << "Data from " << fileAction.session.username << std::endl;
                    file << message.data;
                    continue;
                }

                if (message.type == MessageType::EndCommand)
                {
                    std::cout << "End from " << fileAction.session.username << std::endl;
                    break;
                }

                std::cout << "Unhandled message from " << fileAction.session.username << ": " << buffer << std::endl;
            }

            file.close();
            piper->start(fileAction.session);
        };

        future<void> *execution = (future<void> *)malloc(sizeof(future<void>));
        *execution = async(launch::async, run);
        temporary.queue(execution);
    }
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

    ThreadSafeQueue<FileAction> queue;
    Piper piper(&queue);
    auto queueProcessor = async(launch::async, processQueue, &queue, &piper);

    int clientCounter = 0;
    while (true)
    {
        int clientSocket = awaitConnection(serverSocket);
        int clientId = clientCounter++;

        std::cout << Color::blue << "New client connected. Id: " << clientId << Color::reset << std::endl;

        char buffer[MAX_BUFFER_SIZE];
        bool closeConnection = awaitMessage(&buffer, clientSocket);
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

        piper.start(session);
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
        bool closeConnection = awaitMessage(&buffer, session.socket);

        if (closeConnection)
        {
            std::cout << clientName << " ended connection" << std::endl;
            break;
        }

        Message message = Message::Parse(buffer);

        if (message.type == MessageType::UploadCommand)
        {
            queue->queue(FileAction(session, message.filename, FileActionType::Upload));
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