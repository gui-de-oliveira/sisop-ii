#include <string>

#define MAX_BUFFER_SIZE 1500

void clearBuffer(char (*buffer)[MAX_BUFFER_SIZE]);
bool awaitMessage(char (*buffer)[MAX_BUFFER_SIZE], int socketDescriptor);
void sendMessage(int socket, std::string message);
void sendCustomMessage(int socket);

// client specific methods
int connectToServer(char *address, int port);

// server specific methods
int startServer(int port);
int awaitConnection(int serverSocketDescriptor);
