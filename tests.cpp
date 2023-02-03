#include <iostream>
#include <stdexcept>
#include <list>
#include <future>

enum FileStateTag
{
    Created,
    Reading,
    Updating
};

enum FileOperationTag
{
    Update,
    Read,
    Delete
};

class Connection
{
};

void writeFile(char *path, Connection *connection){

};

void deleteFile(char *path, Connection *connection){

};

using FileOperationFn = std::function<void(char *, Connection *)>;

class FileState
{
protected:
    static void ExecuteOperation(FileOperationTag operation, FileState *lastState, FileState *newState, Connection *connection)
    {
        switch (operation)
        {
        case FileOperationTag::Update:
            switch (lastState->tag)
            {
            case FileStateTag::Created:
                newState->writeFile(newState->name, connection);
                break;

            case FileStateTag::Reading:
                lastState->executingOperation.wait();
                newState->writeFile(newState->name, connection);
                break;

            case FileStateTag::Updating:
                lastState->requestStopUpdate();
                lastState->executingOperation.wait();
                newState->writeFile(newState->name, connection);
                break;
            }
            break;

        case FileOperationTag::Delete:
            switch (lastState->tag)
            {
            case FileStateTag::Created:
                newState->deleteFile(newState->name, connection);
                break;

            case FileStateTag::Reading:
                lastState->executingOperation.wait();
                newState->deleteFile(newState->name, connection);
                break;

            case FileStateTag::Updating:
                lastState->requestStopUpdate();
                lastState->executingOperation.wait();
                newState->deleteFile(newState->name, connection);
                break;
            }
            break;

        case FileOperationTag::Read:
            switch (lastState->tag)
            {
            case FileStateTag::Created:
                newState->readFile(newState->name, connection);
                break;

            case FileStateTag::Reading:
                newState->readFile(newState->name, connection);
                lastState->executingOperation.wait();
                break;

            case FileStateTag::Updating:
                lastState->requestStopUpdate();
                lastState->executingOperation.wait();
                newState->readFile(newState->name, connection);
                break;
            }
            break;
        }
    }

public:
    char *name;
    FileStateTag tag;
    std::future<void> executingOperation;

    FileOperationFn readFile;
    FileOperationFn writeFile;
    FileOperationFn deleteFile;

    FileState(FileOperationFn &_readFile, FileOperationFn &_writeFile, FileOperationFn &_deleteFile)
    {
        readFile = _readFile;
        writeFile = _writeFile;
        deleteFile = _deleteFile;
    }

    FileState(FileState *lastState)
    {
        readFile = lastState->readFile;
    }

    static FileState Update(FileState *lastState, Connection *connection)
    {
        FileState fs(lastState);
        fs.name = lastState->name;
        fs.tag = FileStateTag::Updating;
        fs.executingOperation = std::async(ExecuteOperation, FileOperationTag::Update, lastState, &fs, connection);

        return fs;
    }

    static FileState Read(FileState *lastState, Connection *connection)
    {
        FileState fs(lastState);
        fs.name = lastState->name;
        fs.tag = FileStateTag::Reading;
        fs.executingOperation = std::async(ExecuteOperation, FileOperationTag::Read, lastState, &fs, connection);

        return fs;
    }

    void requestStopUpdate()
    {
    }
};

FileOperationFn fail = [](char *file, Connection *connection)
{
    throw std::runtime_error("FAIL CALLED");
};

void assert(bool expression, std::string message)
{
    if (!expression)
    {
        throw std::runtime_error(message);
    }
}

void test1()
{
    Connection connection;

    bool hasRead = false;

    FileOperationFn readFile =
        [&hasRead](char *a, Connection *b)
    { hasRead = true; };

    FileState initialState(readFile, fail, fail);
    initialState.tag = FileStateTag::Created;

    FileState nextState = FileState::Read(&initialState, &connection);

    nextState.executingOperation.get();
    assert(hasRead, "readFile not called");
}

int main()
{
    std::list<std::function<void()>> tests = {test1};

    int i = 0;
    while (!tests.empty())
    {
        i++;

        std::function<void()> test = tests.front();
        tests.pop_front();

        try
        {
            test();
        }
        catch (std::runtime_error error)
        {
            std::cout << "Failed test " << i << " (message: \"" << error.what() << "\") \n";
        }
    }

    std::cout << "TESTS COMPLETED\n";
}