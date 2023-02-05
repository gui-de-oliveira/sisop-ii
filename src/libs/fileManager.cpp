#include <fstream>

#include "message.h"
#include "fileManager.h"

using namespace std;

FileState getNextState(FileState lastFileState, FileAction fileAction, Callback onComplete)
{
    FileState nextState;

    if (lastFileState.tag == FileStateTag::Empty && fileAction.type == FileActionType::Upload)
    {
        nextState.tag = FileStateTag::Updating;
        nextState.created = fileAction.timestamp;
        nextState.acessed = fileAction.timestamp;
        nextState.updated = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();
        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, onComplete]
            {
                Message::Ok().send(fileAction.session.socket);
                downloadFile(fileAction.session, fileAction.filename, onComplete);
            });

        return nextState;
    }

    else if ((lastFileState.tag == FileStateTag::Updating || lastFileState.tag == FileStateTag::Reading || lastFileState.tag == FileStateTag::Deleting) && fileAction.type == FileActionType::Upload)
    {
        nextState.tag = FileStateTag::Updating;
        nextState.updated = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();
        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, onComplete, lastFileState]
            {
                (lastFileState.executingOperation)->wait();
                Message::Ok().send(fileAction.session.socket);
                downloadFile(fileAction.session, fileAction.filename, onComplete);
            });

        return nextState;
    }

    else if (fileAction.type == FileActionType::Read)
    {

        nextState.tag = FileStateTag::Reading;
        nextState.acessed = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();

        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, onComplete, lastFileState]
            {
                Message::Ok().send(fileAction.session.socket);
                awaitOk(fileAction.session.socket);

                if (lastFileState.tag == FileStateTag::Empty)
                {
                    Message::EndCommand().send(fileAction.session.socket);
                }
                else if (lastFileState.tag == FileStateTag::Reading)
                {
                    sendFile(fileAction.session, fileAction.filename, onComplete);
                    (lastFileState.executingOperation)->wait();
                }
                else
                {
                    (lastFileState.executingOperation)->wait();
                    sendFile(fileAction.session, fileAction.filename, onComplete);
                }
            });

        return nextState;
    }

    throw exception();
}

void downloadFile(Session session, string filename, Callback onComplete)
{
    std::fstream file;
    string filePath = "out/" + session.username + "/" + filename;
    file.open(filePath, ios::out);

    while (true)
    {
        char buffer[MAX_BUFFER_SIZE];
        bool closeConnection = listenPacket(&buffer, session.socket);
        Message::Ok().send(session.socket);

        if (closeConnection)
            break;

        Message message = Message::Parse(buffer);

        if (message.type == MessageType::DataMessage)
        {
            std::cout << session.username << ": [data]" << std::endl;
            file << message.data;
            continue;
        }

        if (message.type == MessageType::EndCommand)
        {
            std::cout << session.username << ": [end]" << std::endl;
            break;
        }

        std::cout << "Unhandled message from " << session.username << ": " << buffer << std::endl;
    }

    file.close();
    onComplete();
    return;
}

void sendFile(Session session, string filename, Callback onComplete)
{
    std::fstream file;
    string filePath = "out/" + session.username + "/" + filename;
    file.open(filePath, ios::in);

    string line;
    std::cout << "Sending file...";
    while (getline(file, line))
    {
        Message::DataMessage(line + "\n").send(session.socket);
        awaitOk(session.socket);
    }
    std::cout << "OK!" << std::endl;
    Message::EndCommand().send(session.socket);

    file.close();
    onComplete();
    return;
}