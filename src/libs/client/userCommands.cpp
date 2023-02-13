#include <iostream>
#include <string>
#include <fstream>

#include "client.h"

using namespace std;

Command Command::Parse(string input)
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
            << Color::blue << message.filename << Color::reset << "\t"
            << toHHMMSS(message.mtime) << "\t"
            << toHHMMSS(message.atime) << "\t"
            << toHHMMSS(message.ctime)
            << endl;

        message = message.Reply(Message::Response(ResponseType::Ok));
    }

    message.Reply(Message::Response(ResponseType::Ok), false);
}