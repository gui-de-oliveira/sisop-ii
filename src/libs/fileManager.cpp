#include <fstream>

#include "message.h"
#include "fileManager.h"

using namespace std;

FileState uploadCommand(FileState lastFileState, FileAction fileAction, Callback onComplete)
{
    FileState nextState;
    nextState.tag = FileStateTag::Updating;
    nextState.updated = fileAction.timestamp;

    if (lastFileState.IsEmptyState() || lastFileState.IsDeletingState())
    {
        nextState.created = fileAction.timestamp;
        nextState.acessed = fileAction.timestamp;
    }

    nextState.executingOperation = allocateFunction();
    *(nextState.executingOperation) = async(
        launch::async,
        [fileAction, onComplete, lastFileState]
        {
            if (lastFileState.tag != FileStateTag::EmptyFile)
            {
                (lastFileState.executingOperation)->wait();
            }

            Message::Response(ResponseType::Ok).send(fileAction.session.socket, false);
            downloadFile(fileAction.session, fileAction.filename);
            onComplete();
        });

    return nextState;
}

FileState deleteCommand(FileState lastFileState, FileAction fileAction, Callback onComplete)
{
    FileState nextState;

    if (!lastFileState.IsEmptyState())
    {
        nextState.tag = FileStateTag::Deleting;
    }

    nextState.executingOperation = allocateFunction();
    *(nextState.executingOperation) = async(
        launch::async,
        [fileAction, onComplete, lastFileState]
        {
            if (lastFileState.tag == FileStateTag::EmptyFile)
            {
                Message::Response(ResponseType::FileNotFound).send(fileAction.session.socket, false);
                onComplete();
                return;
            }

            if (lastFileState.tag != FileStateTag::EmptyFile)
            {
                (lastFileState.executingOperation)->wait();
            }

            Message::Response(ResponseType::Ok).send(fileAction.session.socket, false);
            string path = "out/" + fileAction.session.username + "/" + fileAction.filename;
            deleteFile(fileAction.session, path);
            onComplete();
        });

    return nextState;
}

FileState getNextState(FileState lastFileState, FileAction fileAction, Callback onComplete)
{
    FileState nextState;

    if (fileAction.type == FileActionType::Upload)
    {
        return uploadCommand(lastFileState, fileAction, onComplete);
    }

    if (fileAction.type == FileActionType::Delete)
    {
        return deleteCommand(lastFileState, fileAction, onComplete);
    }

    else if (fileAction.type == FileActionType::Read)
    {

        nextState.tag = FileStateTag::Reading;
        nextState.acessed = fileAction.timestamp;

        nextState.executingOperation = allocateFunction();

        *(nextState.executingOperation) = async(
            launch::async,
            [fileAction, onComplete, &lastFileState]
            {
                Message::Response(ResponseType::Ok).send(fileAction.session.socket);
                awaitOk(fileAction.session.socket);

                if (lastFileState.IsEmptyState())
                {
                    Message::EndCommand().send(fileAction.session.socket);
                    onComplete();
                }
                else if (lastFileState.IsReadingState())
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

void deleteFile(Session session, string path)
{
    Message message = Message::Listen(session.socket);
    if (message.type != MessageType::Start)
    {
        message.panic();
        return;
    }

    remove(path.c_str());
    message.Reply(Message::Response(ResponseType::Ok), false);
}

void downloadFile(Session session, string filename)
{
    std::fstream file;
    string filePath = "out/" + session.username + "/" + filename;
    file.open(filePath, ios::out);

    Message message = Message::Listen(session.socket);
    while (true)
    {
        if (message.type == MessageType::DataMessage)
        {
            std::cout << session.username << ": [data]" << std::endl;
            file << message.data;
            message = message.Reply(Message::Response(ResponseType::Ok));
            continue;
        }

        if (message.type == MessageType::EndCommand)
        {
            std::cout << session.username << ": [end]" << std::endl;
            message.Reply(Message::Response(ResponseType::Ok), false);
            break;
        }

        message.panic();
    }

    file.close();
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