#include <iostream>
#include <unistd.h>
#include <string>
#include <string.h>
#include <fstream>
#include <map>
#include <filesystem>
#include <thread>

#include "libs/message.h"
#include "libs/helpers.h"
#include "libs/fileWatcher.h"

using namespace std;

class ServerConnection
{
public:
    int port;
    char *serverIpAddress;
    std::string username;
    ServerConnection(){};

    ServerConnection(char *serverIpAddress, int port, std::string username)
    {
        this->serverIpAddress = serverIpAddress;
        this->port = port;
        this->username = username;
    }

    Message connect()
    {
        int socket = connectToServer(serverIpAddress, port);
        auto message = Message::Login(username).send(socket);

        if (!message.isOk())
        {
            message.panic();
            throw new std::exception();
        }

        return message;
    }
};

enum FileAction
{
    Created,
    Modified,
    Deleted
};

enum FileOperationTag
{
    ServerUpdate,
    DownloadComplete,
    FailedDownload,
    LocalUpdate
};

string fileActionTagToString(FileAction tag)
{
    switch (tag)
    {
    case FileAction::Created:
        return "Created";
    case FileAction::Modified:
        return "Modified";
    case FileAction::Deleted:
        return "Deleted";

    default:
        return "INVALID_FILE_ACTION";
    }
}

string fileOperationTagToString(FileOperationTag tag)
{
    switch (tag)
    {
    case FileOperationTag::ServerUpdate:
        return "ServerUpdate";
    case FileOperationTag::DownloadComplete:
        return "DownloadComplete";
    case FileOperationTag::FailedDownload:
        return "FailedDownload";
    case FileOperationTag::LocalUpdate:
        return "LocalUpdate";

    default:
        return "INVALID_OPERATION_TAG";
    }
}

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

string toString(FileOperation operation)
{
    std::ostringstream stream;
    std::string timestamp = "[" + toString(operation.timestamp).substr(11, 8) + "]";

    stream << timestamp << " " << fileOperationTagToString(operation.tag) << " " << operation.fileName;

    if (operation.tag == FileOperationTag::LocalUpdate)
    {
        stream << " " << fileActionTagToString(operation.fileAction);
    }

    return stream.str();
}

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

    void setFileState(std::string filename, FileState fileState)
    {
        fileStatesByFilename[filename] = fileState;
    }

    void processEntry(FileOperation entry)
    {
        std::cout << "Execution operation: " << toString(entry) << std::endl;
        auto fileState = getFileState(entry.fileName);

        if (entry.tag == FileOperationTag::ServerUpdate)
        {
            if (fileState.tag == FileStateTag::Inexistent)
            {
                fileState.tag = FileStateTag::Downloading;

                asyncs.queue(
                    [this, entry]
                    {
                        auto message = serverConnection.connect();

                        message = message.Reply(Message::DownloadCommand(entry.fileName));

                        if (!message.isOk())
                        {
                            message.panic();
                            FileOperation operation(FileOperationTag::FailedDownload);
                            queue(operation);
                            return;
                        }

                        std::string user = serverConnection.username;
                        std::string path = "sync_dir_" + user + "/" + entry.fileName;

                        downloadFile(Session(0, message.socket, ""), path);

                        FileOperation operation(FileOperationTag::DownloadComplete);
                        queue(operation);
                    });

                setFileState(entry.fileName, fileState);
            }
        }
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

std::string extractFilenameFromPath(std::string path)
{
    bool hasDirectory = path.find("/") != -1;

    if (!hasDirectory)
    {
        return path;
    }

    int lastDirectory = path.rfind("/");
    return path.substr(lastDirectory + 1);
}

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

class LocalSynchronization
{
    LocalFileStatesManager *localManager;

public:
    LocalSynchronization(string username, LocalFileStatesManager *manager)
    {
        this->localManager = manager;
    }
};

void uploadCommand(int socket, string path)
{

    if (path.length() <= 0)
    {
        cout << Color::red
             << "Missing path arg on upload command.\n"
             << "Expected usage: upload <path/filename.ext>"
             << Color::reset
             << endl;
        return;
    }

    fstream file;
    file.open(path, ios::in);

    if (!file)
    {
        cout << Color::red
             << "Couldn't open file on path \""
             << path
             << "\""
             << Color::reset
             << endl;
        file.close();
        return;
    }

    string filename = extractFilenameFromPath(path);

    Message response = Message::UploadCommand(filename).send(socket);

    if (!response.isOk())
    {
        response.panic();
        return;
    }

    sendFile(Session(0, socket, ""), path);
}

bool isFilenameValid(string filename)
{
    if (filename.length() <= 0)
        return false;

    return true;
}

void downloadCommand(int socket, string filename)
{
    if (!isFilenameValid(filename))
    {
        cout << Color::red
             << "Invalid filename.\n"
             << "Expected usage: download <filename.ext>"
             << Color::reset
             << endl;
        return;
    }

    Message message = Message::DownloadCommand(filename).send(socket);

    if (!message.isOk())
    {
        message.panic();
        return;
    }

    downloadFile(Session(0, socket, ""), filename);
}

void deleteCommand(int socket, string filename)
{
    if (!isFilenameValid(filename))
    {
        cout << Color::red
             << "Invalid filename.\n"
             << "Expected usage: delete <filename.ext>"
             << Color::reset
             << endl;
        return;
    }

    Message deleteResponse = Message::DeleteCommand(filename).send(socket, true);

    if (deleteResponse.responseType == ResponseType::FileNotFound)
    {
        cout << Color::red
             << "File not found"
             << Color::reset
             << endl;
        return;
    }

    if (deleteResponse.type != MessageType::Response ||
        deleteResponse.responseType != ResponseType::Ok)
    {
        deleteResponse.panic();
        return;
    }

    Message response = deleteResponse.Reply(Message::Start());

    if (response.type != MessageType::Response ||
        response.responseType != ResponseType::Ok)
    {
        response.panic();
        return;
    }

    std::cout << Color::green << "File deleted succesfully!" << Color::reset << std::endl;
}

void listServerCommand(int socket)
{
    Message message = Message::ListServerCommand().send(socket);

    if (!message.isOk())
    {
        message.panic();
        return;
    }

    message = message.Reply(Message::Start());

    std::cout
        << "filename\t"
        << "mtime\t"
        << "atime\t"
        << "ctime"
        << endl;

    while (message.type != MessageType::EndCommand)
    {
        if (message.type != MessageType::FileInfo)
        {
            message.panic();
            return;
        }

        std::cout
            << message.filename << "\t"
            << toString(message.mtime) << "\t"
            << toString(message.atime) << "\t"
            << toString(message.ctime)
            << endl;

        message = message.Reply(Message::Response(ResponseType::Ok));
    }

    message.Reply(Message::Response(ResponseType::Ok), false);
}

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

    static Command Parse(string input)
    {

        auto startsWith = [input](string command)
        {
            return input.rfind(command.c_str(), 0) == 0;
        };

        int firstSpace = input.find(" ");
        string parameter = input.substr(firstSpace + 1);

        if (startsWith("upload"))
        {
            return Command(CommandType::Upload, parameter);
        }

        if (startsWith("download"))
        {
            return Command(CommandType::Download, parameter);
        }

        if (startsWith("delete"))
        {
            return Command(CommandType::Delete, parameter);
        }

        if (startsWith("list_server"))
        {
            return Command(CommandType::ListServer, parameter);
        }

        if (startsWith("list_client"))
        {
            return Command(CommandType::ListClient, parameter);
        }

        if (startsWith("get_sync_dir"))
        {
            return Command(CommandType::GetSyncDir, parameter);
        }

        if (startsWith("exit"))
        {
            return Command(CommandType::Exit, parameter);
        }

        return Command(CommandType::InvalidCommand, input);
    }
};

int main(int argc, char *argv[])
{
    // Um cliente deve poder estabelecer uma sessão com o servidor via linha de comando utilizando:
    // ># ./myClient <username> <server_ip_address> <port>
    if (argc != 4)
    {
        cerr << "Expected usage: ./client <username> <server_ip_address> <port>" << endl;
        exit(0);
    }

    // onde <username> representa o identificador do usuário, e <server_ip_address>
    // e <port> representam o endereço IP do servidor e a porta, respectivamente.
    std::string username = argv[1];
    char *serverIpAddress = argv[2];
    int port = atoi(argv[3]);

    ServerConnection serverConnection(serverIpAddress, port, username.c_str());

    std::cout << "Connecting to server..." << std::endl;
    auto message = serverConnection.connect();
    std::cout << "CONNECTED!" << std::endl;

    std::filesystem::remove_all("sync_dir_" + username);
    std::filesystem::create_directory("sync_dir_" + username);

    LocalFileStatesManager manager(serverConnection);
    ServerSynchronization synch(serverConnection, &manager);

    auto onCreate = [&manager](string filename)
    { manager.queue(FileOperation::LocalUpdate(filename, FileAction::Created)); };
    auto onModify = [&manager](string filename)
    { manager.queue(FileOperation::LocalUpdate(filename, FileAction::Modified)); };
    auto onDelete = [&manager](string filename)
    { manager.queue(FileOperation::LocalUpdate(filename, FileAction::Deleted)); };

    auto watchFileChanges = async(launch::async, watch, "sync_dir_" + username, onCreate, onModify, onDelete);

    // Após iniciar uma sessão, o usuário deve ser capaz de arrastar arquivos para o diretório ‘sync_dir’
    // utilizando o gerenciador de arquivos do sistema operacional, e ter esses arquivos sincronizados
    // automaticamente com o servidor e com os demais dispositivos daquele usuário. Da mesma forma, o
    // usuário deve ser capaz de editar ou deletar os arquivos, e ter essas modificações refletidas
    // automaticamente no servidor e nos demais dispositivos daquele usuário.

    // Além disso, uma interface deve ser acessível via linha de comando, permitindo que o usuário realize as
    // operações básicas do sistema, detalhadas na tabela abaixo.

    while (true)
    {
        cout << endl;

        std::string input;
        cout << "Command: ";
        getline(std::cin, input);

        Command command = Command::Parse(input);

        //  upload <path/filename.ext>
        //  Envia o arquivo filename.ext para o servidor, colocando-o no “sync_dir” do
        //  servidor e propagando-o para todos os dispositivos daquele usuário.
        //  e.g. upload /home/user/MyFolder/filename.ext

        if (command.type == CommandType::Upload)
        {
            uploadCommand(message.socket, command.parameter);
            continue;
        }

        // # download <filename.ext> Faz uma cópia não sincronizada do arquivo filename.ext do servidor para
        // o diretório local (de onde o servidor foi chamado). e.g. download
        // mySpreadsheet.xlsx

        if (command.type == CommandType::Download)
        {
            downloadCommand(message.socket, command.parameter);
            continue;
        }

        // # delete <filename.ext> Exclui o arquivo <filename.ext> de “sync_dir”.
        if (command.type == CommandType::Delete)
        {
            deleteCommand(message.socket, command.parameter);
            continue;
        }

        // #list_server Lista os arquivos salvos no servidor associados ao usuário.
        if (command.type == CommandType::ListServer)
        {
            listServerCommand(message.socket);
            continue;
        }

        // # list_client Lista os arquivos salvos no diretório “sync_dir”
        if (command.type == CommandType::ListClient)
        {
            // TODO
            continue;
        }

        // # get_sync_dir Cria o diretório “sync_dir” e inicia as atividades de sincronização
        if (command.type == CommandType::GetSyncDir)
        {
            continue;
        }

        // # exit Fecha a sessão com o servidor
        if (command.type == CommandType::Exit)
        {
            std::cout << "Fechando conexão com o servidor..." << std::endl;
            close(message.socket);
            std::cout << Color::green << "OK!" << Color::reset << std::endl;

            std::cout << "Bye! 🐸" << std::endl;
            return 1;
        }

        cout << Color::red << "Invalid command." << Color::reset << endl;
    }

    char buffer[MAX_BUFFER_SIZE];
    while (true)
    {
        cout << "> ";
        sendCustomPacket(message.socket);

        listenPacket(&buffer, message.socket);
        cout << "< " << buffer << endl;
    }

    close(message.socket);
    cout << "Connection ended" << endl;
    return 0;
}