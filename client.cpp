#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>

using namespace std;
#define MAX_BUFFER_SIZE 1500

int connectToServer(char *serverIpAddress, int port)
{
    sockaddr_in sendSockAddr;
    bzero((char *)&sendSockAddr, sizeof(sendSockAddr));

    sendSockAddr.sin_family = AF_INET;
    sendSockAddr.sin_port = htons(port);

    struct hostent *host = gethostbyname(serverIpAddress);
    sendSockAddr.sin_addr.s_addr =
        inet_addr(inet_ntoa(*(struct in_addr *)*host->h_addr_list));

    int connection = socket(AF_INET, SOCK_STREAM, 0);

    int status = connect(
        connection,
        (sockaddr *)&sendSockAddr,
        sizeof(sendSockAddr));

    if (status < 0)
    {
        cout << "Error connecting to server!" << endl;
        exit(-1);
    }

    return connection;
}

void clearBuffer(char (*buffer)[MAX_BUFFER_SIZE])
{
    memset(buffer, 0, sizeof(*buffer));
}

bool awaitMessage(char (*buffer)[MAX_BUFFER_SIZE], int socketDescriptor)
{
    clearBuffer(buffer);
    int bytesRead = recv(socketDescriptor, (char *)buffer, sizeof(*buffer), 0);

    bool errorOnRead = bytesRead == -1;
    bool isResponseEmpty = bytesRead == 0;
    if (errorOnRead || isResponseEmpty)
    {
        return true;
    }

    return false;
}

void sendMessage(int socket, string message)
{
    char buffer[MAX_BUFFER_SIZE];
    clearBuffer(&buffer);
    strcpy(buffer, message.c_str());
    send(socket, (char *)&buffer, strlen(buffer), 0);
    clearBuffer(&buffer);
}

void sendCustomMessage(int socket)
{
    string data;
    getline(cin, data);
    sendMessage(socket, data);
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