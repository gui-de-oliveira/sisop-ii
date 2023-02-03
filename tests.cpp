#include <iostream>
#include <stdexcept>
#include <list>
#include <future>

enum FileStateTag
{
    Reading,
    Updating,
    Deleting
};

enum FileOperationTag
{
    Create,
    Update,
    Read,
    Delete
};

class Connection
{
};

using FileOperationFn = std::function<void(std::string, Connection *)>;

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
    std::string name;
    FileStateTag tag;
    std::future<void> executingOperation;

    FileOperationFn createFile;
    FileOperationFn writeFile;
    FileOperationFn readFile;
    FileOperationFn deleteFile;

    FileState(){};

    static FileState Create(
        std::string _name,
        Connection *connection,
        FileOperationFn &_createFile,
        FileOperationFn &_readFile,
        FileOperationFn &_writeFile,
        FileOperationFn &_deleteFile)
    {
        FileState fs;

        fs.name = _name;
        fs.tag = FileStateTag::Updating;
        fs.createFile = _createFile;
        fs.writeFile = _writeFile;
        fs.readFile = _readFile;
        fs.deleteFile = _deleteFile;

        fs.executingOperation = std::async(fs.createFile, fs.name, connection);

        return fs;
    }

    FileState(FileState *lastState)
    {
        name = lastState->name;

        createFile = lastState->createFile;
        readFile = lastState->readFile;
        writeFile = lastState->writeFile;
        deleteFile = lastState->deleteFile;
    }

    static FileState Execute(FileOperationTag operation, FileState *lastState, Connection *connection)
    {
        FileState fs(lastState);

        switch (operation)
        {
        case FileOperationTag::Read:
            fs.tag = FileStateTag::Reading;
            break;
        case FileOperationTag::Delete:
            fs.tag = FileStateTag::Deleting;
            break;
        case FileOperationTag::Create:
        case FileOperationTag::Update:
            fs.tag = FileStateTag::Updating;
            break;
        }

        fs.executingOperation = std::async(
            ExecuteOperation,
            operation,
            lastState,
            &fs,
            connection);

        return fs;
    }

    void requestStopUpdate()
    {
    }
};

FileOperationFn fail = [](std::string file, Connection *connection)
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
    Connection expectedConnection;
    std::string expectedFilename = "FOOBAR";

    bool hasExecuted = false;

    FileOperationFn createFile =
        [&hasExecuted, expectedFilename, &expectedConnection](std::string _filename, Connection *_connection)
    {
        assert(expectedFilename == _filename, "Invalid filename");
        assert(&expectedConnection == _connection, "Invalid connection");
        hasExecuted = true;
    };

    FileState state = FileState::Create(expectedFilename, &expectedConnection, createFile, fail, fail, fail);
    state.executingOperation.get();
    assert(hasExecuted, "writeFile not called");
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