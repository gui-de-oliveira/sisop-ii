#include "client.h"

string fileActionTagToString(FileAction tag)
{
    switch (tag)
    {
    case FileAction::Created:
        return "Created";
    case FileAction::Modified:
        return "Modified";
    case FileAction::Deleted:
        return "Deleted";

    default:
        return "INVALID_FILE_ACTION";
    }
}

string fileOperationTagToString(FileOperationTag tag)
{
    switch (tag)
    {
    case FileOperationTag::ServerUpdate:
        return "ServerUpdate";
    case FileOperationTag::DownloadComplete:
        return "DownloadComplete";
    case FileOperationTag::FailedDownload:
        return "FailedDownload";
    case FileOperationTag::LocalUpdate:
        return "LocalUpdate";

    default:
        return "INVALID_OPERATION_TAG";
    }
}

string toString(FileOperation operation)
{
    std::ostringstream stream;
    std::string timestamp = "[" + toString(operation.timestamp).substr(11, 8) + "]";

    stream << timestamp << " " << fileOperationTagToString(operation.tag) << " " << operation.fileName;

    if (operation.tag == FileOperationTag::LocalUpdate)
    {
        stream << " " << fileActionTagToString(operation.fileAction);
    }

    return stream.str();
}

FileState LocalFileStatesManager::nextFileState(FileOperation entry, FileState fileState)
{
    if (entry.tag == FileOperationTag::ServerUpdate)
    {
        return onServerUpdate(entry, fileState);
    }

    return FileState::Inexistent();
}

FileState LocalFileStatesManager::onServerUpdate(FileOperation entry, FileState previousState)
{
    std::string path = "sync_dir_" + serverConnection.username + "/" + entry.fileName;
    FileState nextState;

    if (previousState.tag == FileStateTag::Inexistent)
    {
        nextState.tag = FileStateTag::Downloading;

        asyncs.queue(
            [this, entry, path]
            {
                auto message = serverConnection.connect();

                message = message.Reply(Message::DownloadCommand(entry.fileName));

                if (!message.isOk())
                {
                    message.panic();
                    FileOperation operation(FileOperationTag::FailedDownload);
                    queue(operation);
                    return;
                }

                downloadFile(Session(0, message.socket, ""), path);

                FileOperation operation(FileOperationTag::DownloadComplete);
                queue(operation);
            });
    }

    return nextState;
}