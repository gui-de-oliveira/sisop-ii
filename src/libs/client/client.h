#include <string>
#include <ctime>
#include <map>

#include "../common/helpers.h"
#include "../common/message.h"
#include "fileWatcher.h"

using namespace std;

enum FileAction
{
    Created,
    Modified,
    Deleted
};

string fileActionTagToString(FileAction tag);

enum FileOperationTag
{
    ServerUpdate,
    ServerDelete,
    DownloadComplete,
    Fail,
    UploadCompleted,
    LocalUpdate
};

string fileOperationTagToString(FileOperationTag tag);

class FileOperation
{
public:
    FileOperationTag tag;

    std::string fileName;
    time_t timestamp;
    FileAction fileAction;

    FileOperation(FileOperationTag tag, string filename)
    {
        this->tag = tag;
        this->timestamp = std::time(nullptr);
        this->fileName = filename;
    }

    static FileOperation LocalUpdate(std::string fileName, FileAction fileAction)
    {
        FileOperation operation(FileOperationTag::LocalUpdate, fileName);
        operation.fileName = fileName;
        operation.fileAction = fileAction;
        return operation;
    }

    static FileOperation ServerUpdate(std::string fileName, time_t timestamp)
    {
        FileOperation operation(FileOperationTag::ServerUpdate, fileName);
        operation.fileName = fileName;
        operation.timestamp = timestamp;
        return operation;
    }

    static FileOperation ServerDelete(std::string fileName, time_t timestamp)
    {
        FileOperation operation(FileOperationTag::ServerDelete, fileName);
        operation.fileName = fileName;
        operation.timestamp = timestamp;
        return operation;
    }
};

string toString(FileOperation operation);

enum FileStateTag
{
    Inexistent,
    Downloading,
    Uploading,
    Ready
};

string fileStateTagToString(FileStateTag tag);

class FileState
{
    FileState(FileStateTag tag)
    {
        this->tag = tag;
    }

public:
    FileStateTag tag;
    time_t lastAccessedTime;
    time_t lastModificationTime;
    time_t creationTime;

    time_t endOfDownloadTime;

    FileState()
    {
        this->tag = FileStateTag::Inexistent;
        time_t lastAccessedTime = std::time(nullptr);
        time_t lastModificationTime = std::time(nullptr);
        time_t creationTime = std::time(nullptr);
    }

    static FileState Inexistent()
    {
        return FileState(FileStateTag::Inexistent);
    }
};

class LocalFileStatesManager : public QueueProcessor<FileOperation>
{
    ServerConnection serverConnection;
    std::map<std::string, FileState> fileStatesByFilename;
    AsyncRunner asyncs;

    FileState getFileState(std::string filename)
    {
        if (fileStatesByFilename.find(filename) == fileStatesByFilename.end())
        {
            return FileState::Inexistent();
        }

        return fileStatesByFilename[filename];
    }

    void StartDownload(string filename);
    void StartUpload(string filename);
    void Delete(string filename);

    FileState nextFileState(FileOperation entry, FileState fileState);

    FileState onServerUpdate(FileOperation entry, FileState fileState);
    FileState onServerDelete(FileOperation entry, FileState fileState);
    FileState onDownloadComplete(FileOperation entry, FileState fileState);
    FileState onFail(FileOperation entry, FileState fileState);
    FileState onLocalUpdate(FileOperation entry, FileState fileState);
    FileState onLocalDelete(FileOperation entry, FileState fileState);
    FileState onUploadCompleted(FileOperation entry, FileState fileState);

    void processEntry(FileOperation entry)
    {
        auto currentState = getFileState(entry.fileName);
        auto nextState = nextFileState(entry, currentState);

        std::cout
            << Color::blue << fileStateTagToString(currentState.tag) << Color::reset << " "
            << "[" + toHHMMSS(currentState.lastModificationTime) + "]"
            << " + " << toString(entry)
            << " > "
            << Color::blue << fileStateTagToString(nextState.tag) << Color::reset
            << "[" + toHHMMSS(nextState.lastModificationTime) + "]"
            << std::endl;

        fileStatesByFilename[entry.fileName] = nextState;
    }

public:
    LocalFileStatesManager(ServerConnection serverConnection)
        : QueueProcessor<FileOperation>(
              [this](FileOperation operation)
              { processEntry(operation); })
    {
        this->serverConnection = serverConnection;
    }
};

class ServerSynchronization
{
    ServerConnection serverConnection;
    LocalFileStatesManager *localManager;

    std::future<void> processor;

    void process()
    {
        auto message = serverConnection.connect();

        message = message.Reply(Message::SubscribeUpdates());

        if (!message.isOk())
        {
            std::cout << "ERROR ON SUBSCRIBE TO SERVER" << std::endl;
            message.panic();
            exit(-1);
        }

        message = message.Reply(Message::Start());

        while (true)
        {
            if (message.type == MessageType::EndCommand)
            {
                break;
            }

            if (message.type == MessageType::RemoteFileUpdate)
            {
                FileOperation operation = FileOperation::ServerUpdate(message.filename, message.timestamp);
                localManager->queue(operation);
                message = message.Reply(Message::Response(ResponseType::Ok));
                continue;
            }

            if (message.type == MessageType::RemoteFileDelete)
            {
                FileOperation operation = FileOperation::ServerDelete(message.filename, message.timestamp);
                localManager->queue(operation);
                message = message.Reply(Message::Response(ResponseType::Ok));
                continue;
            }

            message.panic();
            break;
        }

        std::cout << "Server ended connection with subscribe" << std::endl;
        close(message.socket);
    };

public:
    ServerSynchronization(ServerConnection server, LocalFileStatesManager *manager)
    {
        this->serverConnection = server;
        this->localManager = manager;
        this->processor = async(
            launch::async,
            [this]
            { process(); });
    }
};

enum CommandType
{
    InvalidCommand,
    Upload,
    Download,
    Delete,
    ListServer,
    ListClient,
    GetSyncDir,
    Exit
};

class Command
{

public:
    CommandType type;
    std::string parameter;

    Command(CommandType type, string parameter)
    {
        this->type = type;
        this->parameter = parameter;
    }

    static Command Parse(string input);
};

void uploadCommand(int socket, string path);
void downloadCommand(int socket, string filename);
void deleteCommand(int socket, string filename);
void listServerCommand(int socket);
