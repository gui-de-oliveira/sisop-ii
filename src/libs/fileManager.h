#include <future>
#include <map>

#include "helpers.h"
#include "message.h"

enum FileActionType
{
    Upload,
    Read,
    Delete
};

class FileAction
{
public:
    Session session = Session(0, 0, "");
    std::string filename;
    FileActionType type;
    time_t timestamp;

    FileAction(Session _session,
               std::string _filename,
               FileActionType _type,
               time_t _timestamp)
    {
        filename = _filename;
        session = _session;
        type = _type;
        timestamp = _timestamp;
    }
};

enum FileStateTag
{
    EmptyFile,
    Reading,
    Updating,
    Deleting
};

class FileState
{
public:
    FileStateTag tag;
    std::future<void> *executingOperation;

    time_t created;
    time_t updated;
    time_t acessed;

    bool IsEmptyState() { return this->tag == FileStateTag::EmptyFile; }
    bool IsReadingState() { return this->tag == FileStateTag::Reading; }
    bool IsUpdatingState() { return this->tag == FileStateTag::Updating; }
    bool IsDeletingState() { return this->tag == FileStateTag::Deleting; }

    static FileState Empty()
    {
        FileState state;
        state.tag = FileStateTag::EmptyFile;
        return state;
    }
};

class UserFiles
{

public:
    std::map<std::string, FileState> fileStatesByFilename;
    FileState get(std::string filename)
    {
        if (fileStatesByFilename.find(filename) == fileStatesByFilename.end())
        {
            return FileState::Empty();
        }

        return fileStatesByFilename[filename];
    }

    void update(std::string name, FileState state)
    {
        fileStatesByFilename[name] = state;
    }
};

class FilesManager
{
    std::map<std::string, UserFiles *> userFilesByUsername;

public:
    UserFiles *getFiles(std::string username)
    {
        if (userFilesByUsername.find(username) == userFilesByUsername.end())
        {
            UserFiles *initial = (UserFiles *)malloc(sizeof(UserFiles));
            *initial = UserFiles();
            userFilesByUsername[username] = initial;
        }

        return userFilesByUsername[username];
    }
};

std::string toString(FileActionType type);
std::string toString(FileAction fileAction);
std::string toString(FileState fileState);

FileState getNextState(FileState lastFileState, FileAction fileAction, Callback onComplete);
