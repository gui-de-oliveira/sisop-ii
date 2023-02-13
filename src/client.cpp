#include <iostream>
#include <unistd.h>
#include <string>
#include <string.h>
#include <fstream>
#include <map>
#include <filesystem>
#include <thread>

#include "libs/client/client.h"

using namespace std;

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
            manager.queue(FileOperation(FileOperationTag::ListLocalFiles, "---"));
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

    return 0;
}