#include "message.h"
#include "socket.h"

Message::Message()
{
    this->timestamp = time(0);
};

Message::Message(MessageType type)
{
    this->timestamp = time(0);
    this->type = type;
};

Message Message::InvalidMessage() { return Message(MessageType::InvalidMessage); }
Message Message::EndCommand() { return Message(MessageType::EndCommand); }
Message Message::Ok() { return Message(MessageType::Ok); }

Message Message::DataMessage(std::string data)
{
    Message message(MessageType::DataMessage);
    message.data = data;
    return message;
}

Message Message::UploadCommand(std::string filename)
{
    Message message(MessageType::UploadCommand);
    message.filename = filename;
    return message;
}

Message Message::DownloadCommand(std::string filename)
{
    Message message(MessageType::DownloadCommand);
    message.filename = filename;
    return message;
}

Message Message::Parse(char *_buffer)
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

void Message::send(int socket)
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

    sendPacket(socket, message.str());
}

Message listenMessage(int socket)
{
    char buffer[MAX_BUFFER_SIZE];
    listenPacket(&buffer, socket);
    Message::Ok().send(socket);
    return Message::Parse(buffer);
}