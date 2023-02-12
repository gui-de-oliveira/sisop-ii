#include <string.h>
#include <iostream>

#include "socket.h"

enum MessageType
{
    Empty,
    InvalidMessage,
    Login,
    UploadCommand,
    FileUpdate,
    DownloadCommand,
    DeleteCommand,
    EndCommand,
    ListServerCommand,
    SubscribeUpdates,
    FileInfo,
    DataMessage,
    Response,
    Start,
};

enum ResponseType
{
    Invalid,
    Ok,
    FileNotFound,
};

class Message
{
protected:
    Message();
    Message(MessageType type);
    Message(MessageType type, std::string filename);

public:
    MessageType type;

    std::string filename;
    ResponseType responseType;
    std::string data;
    time_t timestamp;

    std::string username;
    int socket;

    time_t mtime;
    time_t atime;
    time_t ctime;

    static Message Empty();
    static Message UploadCommand(std::string filename);
    static Message DownloadCommand(std::string filename);
    static Message DeleteCommand(std::string filename);
    static Message Login(std::string username);
    static Message EndCommand();
    static Message ListServerCommand();
    static Message SubscribeUpdates();
    static Message FileInfo(std::string filename, time_t mtime, time_t atime, time_t ctime);
    static Message FileUpdate(std::string filename, time_t timestamp);
    static Message Response(ResponseType type);
    static Message Start();
    static Message DataMessage(std::string data);
    static Message InvalidMessage();

    static Message Parse(char *_buffer);

    static Message Listen(int socket);
    std::string toPacket();

    Message Reply(Message message, bool expectReply = true);
    Message send(int socket, bool expectReply = true);

    bool isOk();

    void panic();
};

Message listenMessage(int socket);

void deleteFile(Session session, std::string path);
void downloadFile(Session session, std::string path);
void sendFile(Session session, std::string path);