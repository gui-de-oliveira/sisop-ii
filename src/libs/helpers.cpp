#include "helpers.h"
#include <future>
#include <sys/stat.h>
#include <time.h>

using namespace std;

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