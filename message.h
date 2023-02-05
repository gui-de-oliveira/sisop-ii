#include <string.h>
#include <iostream>

enum MessageType
{
    InvalidMessage,
    UploadCommand,
    DownloadCommand,
    DataMessage,
    EndCommand,
    Ok
};

class Message
{
protected:
    Message();
    Message(MessageType type);

public:
    MessageType type;

    std::string filename;
    std::string data;
    time_t timestamp;

    static Message InvalidMessage();
    static Message EndCommand();
    static Message Ok();
    static Message DataMessage(std::string data);
    static Message UploadCommand(std::string filename);
    static Message DownloadCommand(std::string filename);
    static Message Parse(char *_buffer);
    void send(int socket);
};

Message listenMessage(int socket);