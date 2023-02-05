#include <string>
#include <iostream>
#include <ostream>
#include <sstream>

#define MAX_BUFFER_SIZE 1500

class Color
{
public:
    static std::string red;
    static std::string green;
    static std::string blue;
    static std::string yellow;
    static std::string reset;
};

void clearBuffer(char (*buffer)[MAX_BUFFER_SIZE]);
bool listenPacket(char (*buffer)[MAX_BUFFER_SIZE], int socketDescriptor);
void sendPacket(int socket, std::string message);
void sendCustomPacket(int socket);
void awaitOk(int socket);

// client specific methods
int connectToServer(char *address, int port);

// server specific methods
int startServer(int port);
int awaitConnection(int serverSocketDescriptor);
