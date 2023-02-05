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
bool awaitMessage(char (*buffer)[MAX_BUFFER_SIZE], int socketDescriptor);
void sendMessage(int socket, std::string message);
void sendCustomMessage(int socket);

// client specific methods
int connectToServer(char *address, int port);

// server specific methods
int startServer(int port);
int awaitConnection(int serverSocketDescriptor);

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
    Message()
    {
        timestamp = time(0);
    };

public:
    MessageType type;

    std::string filename;
    std::string data;
    time_t timestamp;

    static Message InvalidMessage()
    {
        Message message;
        message.type = MessageType::InvalidMessage;
        return message;
    }

    static Message EndCommand()
    {
        Message message;
        message.type = MessageType::EndCommand;
        return message;
    }

    static Message Ok()
    {
        Message message;
        message.type = MessageType::Ok;
        return message;
    }

    static Message DataMessage(std::string data)
    {
        Message message;
        message.type = MessageType::DataMessage;
        message.data = data;
        return message;
    }

    static Message UploadCommand(std::string filename)
    {
        Message message;
        message.type = MessageType::UploadCommand;
        message.filename = filename;
        return message;
    }

    static Message DownloadCommand(std::string filename)
    {
        Message message;
        message.type = MessageType::DownloadCommand;
        message.filename = filename;
        return message;
    }

    static Message Parse(char *_buffer)
    {
        Message message;

        std::string buffer = _buffer;
        char header = buffer[0];
        std::string data = buffer.substr(2);

        MessageType messageType = (MessageType)atoi(&header);

        switch (messageType)
        {
        case MessageType::Ok:
            return Message::Ok();

        case MessageType::EndCommand:
            return Message::EndCommand();

        case MessageType::DataMessage:
            return Message::DataMessage(data);

        case MessageType::UploadCommand:
            if (data.length() <= 0)
            {
                return Message::InvalidMessage();
            }

            // TODO: Check for invalid characters on filename

            return Message::UploadCommand(data);

        case MessageType::DownloadCommand:
            if (data.length() <= 0)
            {
                return Message::InvalidMessage();
            }

            // TODO: Check for invalid characters on filename

            return Message::DownloadCommand(data);

        default:
            std::cout << "Couldn't parse message: " << buffer << std::endl;
            std::cout << "Type: [" << messageType << "]" << std::endl;
            return Message::InvalidMessage();
        }
    }

    void send(int socket)
    {
        std::ostringstream message;

        message << type << ":";

        switch (type)
        {
        case MessageType::UploadCommand:
        case MessageType::DownloadCommand:
            message << filename;
            break;

        case MessageType::DataMessage:
            message << data;
            break;

        case MessageType::Ok:
        case MessageType::EndCommand:
        case MessageType::InvalidMessage:
            break;
        }

        sendMessage(socket, message.str());
    }
};

Message readMessage(int socket);
void awaitOk(int socket);
