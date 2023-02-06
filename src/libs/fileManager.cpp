#include <fstream>

#include "fileManager.h"

using namespace std;

std::string extractLabelFromPath(std::string path)
{
    bool hasDirectory = path.find("/") != -1;

    if (!hasDirectory)
    {
        return path;
    }

    int lastDirectory = path.rfind("/");
    return path.substr(lastDirectory + 1);
}

std::string toString(FileActionType type)
{
    switch (type)
    {
    case FileActionType::Delete:
        return Color::green + "DELETE" + Color::reset;
    case FileActionType::Read:
        return Color::green + "READ" + Color::reset;
    case FileActionType::Upload:
        return Color::green + "UPLOAD" + Color::reset;
    }

    throw new std::exception;
}

std::string toString(FileAction fileAction)
{
    return toString(fileAction.type) + " OF " +
           fileAction.filename +
           " FROM " +
           Color::yellow +
           fileAction.session.username +
           Color::reset;
}

std::string toString(FileStateTag tag)
{
    switch (tag)
    {
    case FileStateTag::EmptyFile:
        return Color::blue + "EMPTY" + Color::reset;
    case FileStateTag::Deleting:
        return Color::blue + "DELETING" + Color::reset;
    case FileStateTag::Reading:
        return Color::blue + "READING" + Color::reset;
    case FileStateTag::Updating:
        return Color::blue + "UPDATING" + Color::reset;
    }

    throw new std::exception;
}

std::string toString(FileState fileState)
{
    return "TAG: " + toString(fileState.tag);
}

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
            string path = "out/" + fileAction.session.username + "/" + fileAction.filename;
            downloadFile(fileAction.session, path);
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

            (lastFileState.executingOperation)->wait();

            if (lastFileState.tag == FileStateTag::Deleting)
            {
                Message::Response(ResponseType::FileNotFound).send(fileAction.session.socket, false);
                onComplete();
                return;
            }

            Message::Response(ResponseType::Ok).send(fileAction.session.socket, false);
            string path = "out/" + fileAction.session.username + "/" + fileAction.filename;
            deleteFile(fileAction.session, path);
            onComplete();
        });

    return nextState;
}

FileState readCommand(FileState lastFileState, FileAction fileAction, Callback onComplete)
{
    FileState nextState;

    if (!lastFileState.IsEmptyState() && !lastFileState.IsDeletingState())
    {
        nextState.tag = FileStateTag::Reading;
        nextState.acessed = fileAction.timestamp;
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

            if (lastFileState.tag == FileStateTag::Deleting)
            {
                (lastFileState.executingOperation)->wait();
                Message::Response(ResponseType::FileNotFound).send(fileAction.session.socket, false);
                onComplete();
                return;
            }

            if (lastFileState.tag == FileStateTag::Reading || lastFileState.tag == FileStateTag::Updating)
            {

                if (lastFileState.tag == FileStateTag::Updating)
                {
                    (lastFileState.executingOperation)->wait();
                }

                Message::Response(ResponseType::Ok).send(fileAction.session.socket, false);

                string path = "out/" + fileAction.session.username + "/" + fileAction.filename;
                sendFile(fileAction.session, path);

                if (lastFileState.tag == FileStateTag::Reading)
                {
                    (lastFileState.executingOperation)->wait();
                }

                onComplete();
                return;
            }
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

    if (fileAction.type == FileActionType::Read)
    {
        return readCommand(lastFileState, fileAction, onComplete);
    }

    throw exception();
}
