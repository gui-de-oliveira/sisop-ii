#include <future>
#include <sys/stat.h>
#include <time.h>
#include <iomanip>

using namespace std;

#include "helpers.h"

future<void> *allocateFunction()
{
    return (future<void> *)malloc(sizeof(future<void>));
}

time_t getAccessTime(std::string path)
{
    struct stat attrib;
    stat(path.c_str(), &attrib);
    return (attrib.st_atim.tv_sec);
}

time_t getCreateTime(std::string path)
{
    struct stat attrib;
    stat(path.c_str(), &attrib);
    return (attrib.st_ctim.tv_sec);
}

time_t getModificationTime(std::string path)
{
    struct stat attrib;
    stat(path.c_str(), &attrib);
    return (attrib.st_mtim.tv_sec);
}

time_t toTimeT(std::string value)
{
    struct tm tm;
    std::istringstream iss(value);
    iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    time_t time = mktime(&tm);
    return time;
}

std::string toString(time_t value)
{
    std::tm tm = *std::localtime(&value);
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string extractFilenameFromPath(std::string path)
{
    bool hasDirectory = path.find("/") != -1;

    if (!hasDirectory)
    {
        return path;
    }

    int lastDirectory = path.rfind("/");
    return path.substr(lastDirectory + 1);
}

bool isFilenameValid(string filename)
{
    if (filename.length() <= 0)
        return false;

    return true;
}