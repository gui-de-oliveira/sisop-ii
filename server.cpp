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
    time_t timestamp;

    FileAction(Session _session,
               string _filename,
               FileActionType _type,
               time_t _timestamp)
    {
        filename = _filename;
        session = _session;
        type = _type;
        timestamp = _timestamp;
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
enum FileStateTag
{
    Empty,
    Reading,
    Updating,
    Deleting
};

class FileState
{
public:
    FileStateTag tag;
    std::future<void> *executingOperation;

    time_t created;
    time_t updated;
    time_t acessed;

    static FileState Empty()
    {
        FileState state;
        state.tag = FileStateTag::Empty;
        return state;
    }
};

class UserFiles
{

public:
    map<string, FileState> fileStatesByFilename;
    FileState get(string filename)
    {
        if (fileStatesByFilename.find(filename) == fileStatesByFilename.end())
        {
            return FileState::Empty();
        }

        return fileStatesByFilename[filename];
    }

    void update(string name, FileState state)
    {
        fileStatesByFilename[name] = state;
    }
};

class FilesManager
{
    map<string, UserFiles *> userFilesByUsername;

public:
    UserFiles *getFiles(string username)
    {
        if (userFilesByUsername.find(username) == userFilesByUsername.end())
        {
            UserFiles *initial = (UserFiles *)malloc(sizeof(UserFiles));
            *initial = UserFiles();
            userFilesByUsername[username] = initial;
        }

        return userFilesByUsername[username];
    }
};

FilesManager fileManager;

void downloadFile(Session session, string filename, Piper *piper)
{
    std::fstream file;
    string filePath = "out/" + session.username + "/" + filename;
    file.open(filePath, ios::out);

    while (true)
    {
        char buffer[MAX_BUFFER_SIZE];
        bool closeConnection = awaitMessage(&buffer, session.socket);
        Message::Ok().send(session.socket);

        if (closeConnection)
            break;

        Message message = Message::Parse(buffer);

        if (message.type == MessageType::DataMessage)
        {
            std::cout << session.username << ": [data]" << std::endl;
            file << message.data;
            continue;
        }

        if (message.type == MessageType::EndCommand)
        {
            std::cout << session.username << ": [end]" << std::endl;
            break;
        }

        std::cout << "Unhandled message from " << session.username << ": " << buffer << std::endl;
    }

    file.close();
    piper->start(session);
    return;
}

void sendFile(Session session, string filename, Piper *piper)
{
    std::fstream file;
    string filePath = "out/" + session.username + "/" + filename;
    file.open(filePath, ios::in);

    string line;
    std::cout << "Sending file...";
    while (getline(file, line))
    {
        Message::DataMessage(line + "\n").send(session.socket);
        awaitOk(session.socket);
    }
    std::cout << "OK!" << std::endl;
    Message::EndCommand().send(session.socket);

    file.close();
    piper->start(session);
    return;
}

future<void> *allocateFunction()
{
    return (future<void> *)malloc(sizeof(future<void>));
}

FileState getNextState(FileState lastFileState, FileAction fileAction, Piper *piper)
{

    FileState nextState;

    if (lastFileState.tag == FileStateTag::Empty && fileAction.type == FileActionType::Upload)
    {
        nextState.tag = FileStateTag::Updating;
        nextState.created = fileAction.timestamp;
        nextState.acessed = fileAction.timestamp;
        nextState.updated = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();
        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, piper]
            {
                Message::Ok().send(fileAction.session.socket);
                downloadFile(fileAction.session, fileAction.filename, piper);
            });

        return nextState;
    }

    else if ((lastFileState.tag == FileStateTag::Updating || lastFileState.tag == FileStateTag::Reading || lastFileState.tag == FileStateTag::Deleting) && fileAction.type == FileActionType::Upload)
    {
        nextState.tag = FileStateTag::Updating;
        nextState.updated = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();
        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, piper, lastFileState]
            {
                (lastFileState.executingOperation)->wait();
                Message::Ok().send(fileAction.session.socket);
                downloadFile(fileAction.session, fileAction.filename, piper);
            });

        return nextState;
    }

    else if (fileAction.type == FileActionType::Read)
    {

        nextState.tag = FileStateTag::Reading;
        nextState.acessed = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();

        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, piper, lastFileState]
            {
                Message::Ok().send(fileAction.session.socket);
                awaitOk(fileAction.session.socket);

                if (lastFileState.tag == FileStateTag::Empty)
                {
                    Message::EndCommand().send(fileAction.session.socket);
                }
                else if (lastFileState.tag == FileStateTag::Reading)
                {
                    sendFile(fileAction.session, fileAction.filename, piper);
                    (lastFileState.executingOperation)->wait();
                }
                else
                {
                    (lastFileState.executingOperation)->wait();
                    sendFile(fileAction.session, fileAction.filename, piper);
                }
            });

        return nextState;
    }

    throw exception();
}

void processQueue(ThreadSafeQueue<FileAction> *queue, Piper *piper)
{
    while (true)
    {
        FileAction fileAction = queue->pop();

        string username = fileAction.session.username;

        UserFiles *userFiles = fileManager.getFiles(username);
        FileState lastFileState = userFiles->get(fileAction.filename);
        userFiles->fileStatesByFilename[fileAction.filename] = getNextState(lastFileState, fileAction, piper);
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