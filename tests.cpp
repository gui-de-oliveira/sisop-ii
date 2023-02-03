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

        if (operation == FileOperationTag::Update && lastState->tag == FileStateTag::Reading)
        {
            lastState->executingOperation.wait();
            newState->writeFile(newState->name, connection);
        }
        else if (operation == FileOperationTag::Update && lastState->tag == FileStateTag::Updating)
        {
            lastState->requestStopUpdate();
            lastState->executingOperation.wait();
            newState->writeFile(newState->name, connection);
        }
        else if (operation == FileOperationTag::Update && lastState->tag == FileStateTag::Deleting)
        {
            lastState->executingOperation.wait();
            newState->createFile(newState->name, connection);
        }

        if (operation == FileOperationTag::Read && lastState->tag == FileStateTag::Reading)
        {
            newState->readFile(newState->name, connection);
            lastState->executingOperation.wait();
        }
        else if (operation == FileOperationTag::Read && lastState->tag == FileStateTag::Updating)
        {
            lastState->executingOperation.wait();
            newState->readFile(newState->name, connection);
        }
        else if (operation == FileOperationTag::Read && lastState->tag == FileStateTag::Deleting)
        {
            // return not found
            lastState->executingOperation.wait();
        }

        if (operation == FileOperationTag::Delete && lastState->tag == FileStateTag::Reading)
        {
            lastState->executingOperation.wait();
            newState->deleteFile(newState->name, connection);
        }
        else if (operation == FileOperationTag::Delete && lastState->tag == FileStateTag::Updating)
        {
            lastState->requestStopUpdate();
            lastState->executingOperation.wait();
            newState->deleteFile(newState->name, connection);
        }
        else if (operation == FileOperationTag::Delete && lastState->tag == FileStateTag::Deleting)
        {
            lastState->executingOperation.wait();
            // close connection but dont delete file again
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
            if (lastState->tag == FileStateTag::Deleting)
            {
                fs.tag = FileStateTag::Deleting;
            }
            else
            {
                fs.tag = FileStateTag::Reading;
            }
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
    assert(state.tag == FileStateTag::Updating, "expected updating tag");
    assert(hasExecuted, "writeFile not called");
}

void test2()
{
    Connection expectedConnection;
    std::string expectedFilename = "FOOBAR";

    bool hasCreateFileExecuted = false;
    bool hasReadFileExecuted = false;

    FileOperationFn createFile =
        [&hasCreateFileExecuted, hasReadFileExecuted, expectedFilename, &expectedConnection](std::string _filename, Connection *_connection)
    {
        assert(expectedFilename == _filename, "Invalid filename");
        assert(&expectedConnection == _connection, "Invalid connection");
        assert(!hasReadFileExecuted, "Read file executed before create file");
        hasCreateFileExecuted = true;
    };

    FileOperationFn readFile =
        [&hasCreateFileExecuted, &hasReadFileExecuted, expectedFilename, &expectedConnection](std::string _filename, Connection *_connection)
    {
        assert(hasCreateFileExecuted, "Create file has not executed before read file");
        assert(expectedFilename == _filename, "Invalid filename");
        assert(&expectedConnection == _connection, "Invalid connection");
        hasReadFileExecuted = true;
    };

    FileState initialState = FileState::Create(expectedFilename, &expectedConnection, createFile, readFile, fail, fail);
    assert(initialState.tag == FileStateTag::Updating, "expected updating tag");

    FileState nextState = initialState.Execute(FileOperationTag::Read, &initialState, &expectedConnection);
    assert(nextState.tag == FileStateTag::Reading, "expected reading tag");

    nextState.executingOperation.get();
    assert(hasReadFileExecuted, "read file was not executed");
}

void test3()
{
    Connection expectedConnection;
    std::string expectedFilename = "FOOBAR";

    bool hasCreateFileExecuted = false;
    bool hasWriteFileExecuted = false;

    FileOperationFn createFile =
        [&hasCreateFileExecuted, hasWriteFileExecuted, expectedFilename, &expectedConnection](std::string _filename, Connection *_connection)
    {
        assert(expectedFilename == _filename, "Invalid filename");
        assert(&expectedConnection == _connection, "Invalid connection");
        assert(!hasWriteFileExecuted, "Read file executed before create file");
        hasCreateFileExecuted = true;
    };

    FileOperationFn writeFile =
        [&hasCreateFileExecuted, &hasWriteFileExecuted, expectedFilename, &expectedConnection](std::string _filename, Connection *_connection)
    {
        assert(hasCreateFileExecuted, "Create file has not executed before read file");
        assert(expectedFilename == _filename, "Invalid filename");
        assert(&expectedConnection == _connection, "Invalid connection");
        hasWriteFileExecuted = true;
    };

    FileState initialState = FileState::Create(expectedFilename, &expectedConnection, createFile, fail, writeFile, fail);
    FileState nextState = initialState.Execute(FileOperationTag::Update, &initialState, &expectedConnection);

    nextState.executingOperation.get();
    assert(hasWriteFileExecuted, "write file was not executed");
}

int main()
{
    std::list<std::function<void()>> tests = {
        test1,
        test2,
        test3};

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