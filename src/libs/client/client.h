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
    DownloadComplete,
    FailedDownload,
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

    FileOperation(FileOperationTag tag)
    {
        this->tag = tag;
        this->timestamp = std::time(nullptr);
    }

    static FileOperation LocalUpdate(std::string fileName, FileAction fileAction)
    {
        FileOperation operation(FileOperationTag::LocalUpdate);
        operation.fileName = fileName;
        operation.fileAction = fileAction;
        return operation;
    }

    static FileOperation ServerUpdate(std::string fileName, time_t timestamp)
    {
        FileOperation operation(FileOperationTag::ServerUpdate);
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
    Ready
};

class FileState
{
    FileState(FileStateTag tag)
    {
        this->tag = tag;
    }

public:
    FileStateTag tag;

    FileState()
    {
        this->tag = FileStateTag::Inexistent;
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

    FileState nextFileState(FileOperation entry, FileState fileState);
    FileState onServerUpdate(FileOperation entry, FileState fileState);

    void processEntry(FileOperation entry)
    {
        std::cout << "Execution operation: " << toString(entry) << std::endl;
        auto currentState = getFileState(entry.fileName);
        auto nextState = nextFileState(entry, currentState);
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

            if (message.type == MessageType::FileUpdate)
            {
                std::cout << Color::blue << "Update received on file " << message.filename << Color::reset << std::endl;
                FileOperation operation = FileOperation::ServerUpdate(message.filename, message.timestamp);
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
