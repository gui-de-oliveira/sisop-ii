#include <iostream>
#include <unistd.h>
#include <string>
#include <string.h>
#include <fstream>

#include "libs/socket.h"
#include "libs/message.h"

using namespace std;

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

    if (response.type != MessageType::Response ||
        response.responseType != ResponseType::Ok)
    {
        response.panic();
        return;
    }

    string line;
    std::cout << "Sending file...";
    while (getline(file, line))
    {
        auto response = Message::DataMessage(line + "\n").send(socket);

        if (response.type != MessageType::Response ||
            response.responseType != ResponseType::Ok)
        {
            response.panic();
            file.close();
            return;
        }
    }
    std::cout << "OK!" << std::endl;
    file.close();

    auto ack = Message::EndCommand().send(socket);
    if (ack.type != MessageType::Response ||
        ack.responseType != ResponseType::Ok)
    {
        ack.panic();
    }
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

    fstream file;
    file.open(filename, ios::out);

    if (!file)
    {
        cout << Color::red
             << "Couldn't open file on path \""
             << filename
             << "\""
             << Color::reset
             << endl;
        file.close();
        return;
    }

    Message::DownloadCommand(filename).send(socket);
    awaitOk(socket);

    Message::Response(ResponseType::Ok).send(socket);

    while (true)
    {
        Message message = listenMessage(socket);

        if (message.type == MessageType::DataMessage)
        {
            file << message.data;
            std::cout << "Received data" << endl;
            continue;
        }

        if (message.type == MessageType::EndCommand)
        {
            break;
        }

        std::cout << "Unhandled message " << message.type << endl;
    }

    file.close();
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

enum CommandType
{
    InvalidCommand,
    Upload,
    Download,
    Delete,
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
    char *username = argv[1];
    char *serverIpAddress = argv[2];
    int port = atoi(argv[3]);

    cout << "Connecting to server... ";
    int socket = connectToServer(serverIpAddress, port);
    cout << "OK!" << endl;

    cout << "Logging as " << username << "... ";
    auto message = Message::Login(username).send(socket);
    if (!message.isOk())
    {
        message.panic();
        return -1;
    }
    cout << "OK!" << endl;

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
            uploadCommand(socket, command.parameter);
            continue;
        }

        // # download <filename.ext> Faz uma cópia não sincronizada do arquivo filename.ext do servidor para
        // o diretório local (de onde o servidor foi chamado). e.g. download
        // mySpreadsheet.xlsx

        if (command.type == CommandType::Download)
        {
            downloadCommand(socket, command.parameter);
            continue;
        }

        if (command.type == CommandType::Delete)
        {
            deleteCommand(socket, command.parameter);
            continue;
        }

        cout << Color::red << "Invalid command." << Color::reset << endl;
    }

    char buffer[MAX_BUFFER_SIZE];
    while (true)
    {
        cout << "> ";
        sendCustomPacket(socket);

        listenPacket(&buffer, socket);
        cout << "< " << buffer << endl;
    }

    close(socket);
    cout << "Connection ended" << endl;
    return 0;
}