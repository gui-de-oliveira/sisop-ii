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
    case FileOperationTag::UploadCompleted:
        return "UploadCompleted";

    default:
        return "INVALID_OPERATION_TAG";
    }
}

string fileStateTagToString(FileStateTag tag)
{
    switch (tag)
    {
    case FileStateTag::Inexistent:
        return "Inexistent";
    case FileStateTag::Downloading:
        return "Downloading";
    case FileStateTag::Uploading:
        return "Uploading";
    case FileStateTag::Ready:
        return "Ready";

    default:
        return "INVALID_OPERATION_TAG";
    }
}

string toString(FileOperation operation)
{
    std::ostringstream stream;
    std::string timestamp = "[" + toHHMMSS(operation.timestamp) + "]";

    stream
        << Color::yellow << fileOperationTagToString(operation.tag) << Color::reset
        << " "
        << timestamp
        << " " << operation.fileName;

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

    if (entry.tag == FileOperationTag::DownloadComplete)
    {
        return onDownloadComplete(entry, fileState);
    }

    if (entry.tag == FileOperationTag::FailedDownload)
    {
        return onFailedDownload(entry, fileState);
    }

    if (entry.tag == FileOperationTag::LocalUpdate)
    {
        return onLocalUpdate(entry, fileState);
    }

    if (entry.tag == FileOperationTag::UploadCompleted)
    {
        return onUploadCompleted(entry, fileState);
    }
    return FileState::Inexistent();
}

void LocalFileStatesManager::StartDownload(string filename)
{
    auto download = [this, filename]
    {
        std::string path = "sync_dir_" + serverConnection.username + "/" + filename;

        auto message = serverConnection.connect();

        message = message.Reply(Message::DownloadCommand(filename));

        if (!message.isOk())
        {
            message.panic();
            FileOperation operation(FileOperationTag::FailedDownload);
            queue(operation);
            return;
        }

        downloadFile(Session(0, message.socket, ""), path);
        close(message.socket);
        FileOperation operation = FileOperation::DownloadComplete(filename);
        queue(operation);
    };

    asyncs.queue(download);
}

void LocalFileStatesManager::StartUpload(string filename)
{
    auto upload = [this, filename]
    {
        std::string path = "sync_dir_" + serverConnection.username + "/" + filename;

        auto message = serverConnection.connect();

        message = message.Reply(Message::UploadCommand(filename));

        if (!message.isOk())
        {
            message.panic();
            FileOperation operation(FileOperationTag::FailedDownload);
            queue(operation);
            return;
        }

        sendFile(Session(0, message.socket, ""), path);
        close(message.socket);
        FileOperation operation = FileOperation::UploadCompleted(filename);
        queue(operation);
    };

    asyncs.queue(upload);
}

void LocalFileStatesManager::Delete(string filename)
{
    auto deleteFn = [this, filename]
    {
        auto message = serverConnection.connect();

        message = message.Reply(Message::DeleteCommand(filename));

        if (!message.isOk())
        {
            message.panic();
            FileOperation operation(FileOperationTag::FailedDownload);
            queue(operation);
            return;
        }

        message.Reply(Message::Start());
        close(message.socket);
    };

    asyncs.queue(deleteFn);
}

FileState LocalFileStatesManager::onServerUpdate(FileOperation entry, FileState previousState)
{
    FileState nextState = previousState;

    if (previousState.tag == FileStateTag::Inexistent)
    {
        nextState.tag = FileStateTag::Downloading;
        nextState.creationTime = now();
        nextState.lastAccessedTime = now();
        nextState.lastModificationTime = now();
        StartDownload(entry.fileName);
        return nextState;
    }

    if (previousState.tag == FileStateTag::Ready)
    {
        if (previousState.lastModificationTime == entry.timestamp)
        {
            return previousState;
        }

        if (previousState.lastModificationTime < entry.timestamp)
        {
            nextState.tag = FileStateTag::Downloading;
            nextState.lastModificationTime = now();
            StartDownload(entry.fileName);
            return nextState;
        }

        if (previousState.lastModificationTime > entry.timestamp)
        {
            // OUTDATED VALUE ON SERVER => UPLOAD LOCAL VERSION
            return nextState;
        }
    }

    if (previousState.tag == FileStateTag::Uploading)
    {
        // TODO
    }

    if (previousState.tag == FileStateTag::Downloading)
    {
        // ALREADY DOWNLOADING, IGNORE... for now
        return previousState;
    }

    return nextState;
}

FileState LocalFileStatesManager::onDownloadComplete(FileOperation entry, FileState previousState)
{
    FileState nextState = previousState;

    if (previousState.tag == FileStateTag::Inexistent)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Ready)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Uploading)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Downloading)
    {
        nextState.tag = FileStateTag::Ready;
        nextState.endOfDownloadTime = now();
        return nextState;
    }

    return nextState;
}

FileState LocalFileStatesManager::onFailedDownload(FileOperation entry, FileState previousState)
{
    FileState nextState = previousState;

    if (previousState.tag == FileStateTag::Inexistent)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Ready)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Uploading)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Downloading)
    {
        return FileState::Inexistent();
    }

    return nextState;
}

FileState LocalFileStatesManager::onLocalUpdate(FileOperation entry, FileState previousState)
{
    FileState nextState = previousState;

    if (entry.fileAction == FileAction::Deleted)
    {
        return onLocalDelete(entry, previousState);
    }

    if (previousState.tag == FileStateTag::Inexistent)
    {
        nextState.creationTime = now();
        nextState.lastAccessedTime = now();
        nextState.lastModificationTime = now();
        nextState.tag = FileStateTag::Uploading;
        StartUpload(entry.fileName);
        return nextState;
    }

    if (previousState.tag == FileStateTag::Ready)
    {
        if (previousState.lastModificationTime == entry.timestamp)
        {
            return previousState;
        }

        if (previousState.endOfDownloadTime + 3 > entry.timestamp)
        {
            // change made by the download, ignore
            return previousState;
        }

        if (previousState.lastModificationTime < entry.timestamp)
        {
            nextState.lastModificationTime = now();
            nextState.tag = FileStateTag::Uploading;
            StartUpload(entry.fileName);
            return nextState;
        }

        if (previousState.lastModificationTime > entry.timestamp)
        {
            return previousState;
        }
    }

    if (previousState.tag == FileStateTag::Downloading)
    {
        // changes made by the download
        return previousState;
    }

    if (previousState.tag == FileStateTag::Uploading)
    {
        // File sent is outdated, check for a resend
    }

    return nextState;
}

FileState LocalFileStatesManager::onLocalDelete(FileOperation entry, FileState previousState)
{
    FileState nextState = previousState;

    if (previousState.tag == FileStateTag::Inexistent)
    {
    }

    if (previousState.tag == FileStateTag::Ready)
    {
    }

    if (previousState.tag == FileStateTag::Downloading)
    {
    }

    if (previousState.tag == FileStateTag::Uploading)
    {
    }

    Delete(entry.fileName);
    nextState.tag = FileStateTag::Inexistent;

    return nextState;
}

FileState LocalFileStatesManager::onUploadCompleted(FileOperation entry, FileState previousState)
{
    FileState nextState = previousState;

    if (previousState.tag == FileStateTag::Inexistent)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Ready)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Downloading)
    {
        // Invalid combination
    }

    if (previousState.tag == FileStateTag::Uploading)
    {
        nextState.tag = FileStateTag::Ready;
        return nextState;
    }

    return nextState;
}