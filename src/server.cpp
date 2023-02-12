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
#include <algorithm>
#include <iterator>
#include <optional>

#include "libs/fileManager.h"

using namespace std;

void expectFileAction(Session, ThreadSafeQueue<FileAction> *);

class Singleton
{
protected:
public:
    AsyncRunner *runner;
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
            [session, this]
            { expectFileAction(session, fileQueue); });
    }
};

void processQueue(Singleton *singleton)
{
    while (true)
    {
        std::optional<FileAction> result = singleton->fileQueue->pop();

        if (!result.has_value())
        {
            sleep(1);
            continue;
        }

        FileAction fileAction = result.value();
        std::cout << "BEGIN: " << toString(fileAction) << endl;

        string username = fileAction.session.username;

        UserFiles *userFiles = singleton->fileManager->getFiles(username);

        std::list<int> subscribers = *(userFiles->subscribers);
        Callback onComplete = [fileAction, singleton, subscribers]()
        {
            std::cout << "END: " << toString(fileAction) << endl;
            singleton->start(fileAction.session);

            if (fileAction.type == FileActionType::Delete ||
                fileAction.type == FileActionType::Upload)
            {
                for (auto const &subscriber : subscribers)
                {
                    Message::FileUpdate(fileAction.filename, fileAction.timestamp).send(subscriber);
                }
            }
        };

        if (fileAction.type == FileActionType::Subscribe)
        {
            list<Message> fileUpdates;

            for (auto const &item : userFiles->fileStatesByFilename)
            {
                auto name = item.first;
                auto state = item.second;
                fileUpdates.push_front(Message::FileUpdate(name, state.updated));
            }

            userFiles->subscribers->push_front(fileAction.session.socket);

            auto sendFileUpdates = [fileAction, fileUpdates, onComplete]
            {
                auto message = Message::Response(ResponseType::Ok).send(fileAction.session.socket);

                if (message.type != MessageType::Start)
                {
                    message.panic();
                    return;
                }

                for (auto const &fileInfo : fileUpdates)
                {
                    message = message.Reply(fileInfo);

                    if (!message.isOk())
                    {
                        message.panic();
                        return;
                    }
                }
            };

            singleton->runner->queue(
                [sendFileUpdates]
                { sendFileUpdates(); });
            continue;
        }

        if (fileAction.type == FileActionType::ListServer)
        {
            list<Message> fileInfos;

            for (auto const &item : userFiles->fileStatesByFilename)
            {
                auto name = item.first;
                auto state = item.second;
                fileInfos.push_front(Message::FileInfo(name, state.updated, state.acessed, state.created));
            }

            auto sendFileInfos = [fileAction, fileInfos, onComplete]
            {
                auto message = Message::Response(ResponseType::Ok).send(fileAction.session.socket);

                if (message.type != MessageType::Start)
                {
                    message.panic();
                    return;
                }

                for (auto const &fileInfo : fileInfos)
                {
                    message = message.Reply(fileInfo);

                    if (!message.isOk())
                    {
                        message.panic();
                        return;
                    }
                }

                message = message.Reply(Message::EndCommand());

                if (!message.isOk())
                {
                    message.panic();
                    return;
                }

                onComplete();
            };

            singleton->runner->queue(
                [sendFileInfos]
                { sendFileInfos(); });
            continue;
        }

        FileState lastFileState = userFiles->get(fileAction.filename);

        auto nextState = getNextState(lastFileState, fileAction, onComplete);
        std::cout << toString(lastFileState) << " > " << toString(nextState) << endl;

        userFiles->fileStatesByFilename[fileAction.filename] = nextState;
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
        std::cout << Color::yellow << "Awaiting new connections: " << Color::reset << std::endl;
        int clientSocket = awaitConnection(serverSocket);
        int clientId = clientCounter++;

        std::cout << Color::blue
                  << "New client connected on socket " << clientSocket
                  << ". Id: " << clientId
                  << Color::reset << std::endl;

        auto login = Message::Listen(clientSocket);

        if (login.type != MessageType::Login)
        {
            std::cout << "Login failed for Client id " << clientId << std::endl;
            login.panic();
            close(clientSocket);
            continue;
        }

        login.Reply(Message::Response(ResponseType::Ok), false);

        string username = login.username;
        string folder = "out/" + username + "/";
        mkdir(folder.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        std::cout << "Client " << clientId << " logged in as " << username << std::endl;
        Session session(clientId, clientSocket, username);

        singleton.start(session);
    }

    return 0;
}

void expectFileAction(Session session, ThreadSafeQueue<FileAction> *queue)
{
    std::ostringstream clientNameStream;
    clientNameStream << Color::yellow << "[" << session.clientId << "]" << Color::reset;
    std::string clientName = clientNameStream.str();

    while (true)
    {
        Message message = Message::Listen(session.socket);
        std::cout << clientName << " queued " << message.type << std::endl;

        if (message.type == MessageType::SubscribeUpdates)
        {
            queue->queue(FileAction(session, "", FileActionType::Subscribe, message.timestamp));
            return;
        }

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

        if (message.type == MessageType::DeleteCommand)
        {
            queue->queue(FileAction(session, message.filename, FileActionType::Delete, message.timestamp));
            return;
        }

        if (message.type == MessageType::ListServerCommand)
        {
            queue->queue(FileAction(session, "", FileActionType::ListServer, message.timestamp));
            return;
        }

        if (message.type == MessageType::Empty)
        {
            break;
        }

        message.panic();
    }

    close(session.socket);
    std::cout << "Connection with " << clientName << " closed" << std::endl;
}