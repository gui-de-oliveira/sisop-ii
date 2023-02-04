#include <iostream>
#include <unistd.h>
#include <string>
#include <string.h>
#include <fstream>

#include "socket.h"

using namespace std;

std::string extractFilenameFromPath(std::string path)
{
    int lastDirectory = path.rfind("/");
    return path.substr(lastDirectory + 1);
}

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
    sendMessage(socket, username);
    cout << "OK!" << endl;

    // Após iniciar uma sessão, o usuário deve ser capaz de arrastar arquivos para o diretório ‘sync_dir’
    // utilizando o gerenciador de arquivos do sistema operacional, e ter esses arquivos sincronizados
    // automaticamente com o servidor e com os demais dispositivos daquele usuário. Da mesma forma, o
    // usuário deve ser capaz de editar ou deletar os arquivos, e ter essas modificações refletidas
    // automaticamente no servidor e nos demais dispositivos daquele usuário.

    // Além disso, uma interface deve ser acessível via linha de comando, permitindo que o usuário realize as
    // operações básicas do sistema, detalhadas na tabela abaixo.

    auto awaitOk = [socket]()
    {
        char buffer[MAX_BUFFER_SIZE];
        awaitMessage(&buffer, socket);
    };

    while (true)
    {
        cout << endl;

        std::string input;
        cout << "Command: ";
        getline(std::cin, input);

        int firstSpace = input.find(" ");
        string command = input.substr(0, firstSpace);

        //  upload <path/filename.ext>
        //  Envia o arquivo filename.ext para o servidor, colocando-o no “sync_dir” do
        //  servidor e propagando-o para todos os dispositivos daquele usuário.
        //  e.g. upload /home/user/MyFolder/filename.ext

        string uploadCommand = "upload";
        bool isUploadCommand = strcmp(uploadCommand.c_str(), command.c_str()) == 0;
        if (isUploadCommand)
        {
            string path = input.substr(uploadCommand.length() + 1);

            if (path.length() <= 0)
            {
                cout << Color::red
                     << "Missing path arg on upload command.\n"
                     << "Expected usage: upload <path/filename.ext>"
                     << Color::reset
                     << endl;
                continue;
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
                continue;
            }

            string filename = extractFilenameFromPath(path);
            Message::UploadCommand(filename).send(socket);
            awaitOk();

            string line;
            std::cout << "Sending file...";
            while (getline(file, line))
            {
                Message::DataMessage(line + "\n").send(socket);
                awaitOk();
            }
            std::cout << "OK!" << std::endl;

            Message::EndCommand().send(socket);
            awaitOk();

            file.close();
            continue;
        }

        cout << Color::red << "Invalid \"" << command << "\" command." << Color::reset << endl;
    }

    char buffer[MAX_BUFFER_SIZE];
    while (true)
    {
        cout << "> ";
        sendCustomMessage(socket);

        awaitMessage(&buffer, socket);
        cout << "< " << buffer << endl;
    }

    close(socket);
    cout << "Connection ended" << endl;
    return 0;
}