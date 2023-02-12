#include <future>
#include <list>
#include <unistd.h>
#include <iostream>
#include <iostream>

using namespace std;

#define Callback std::function<void()>

template <typename T>
class ThreadSafeQueue
{
protected:
    list<T> queued;

public:
    void queue(T data)
    {
        queued.push_back(data);
    }

    T pop()
    {
        while (queued.empty())
        {
            sleep(1);
        }

        T data = queued.front();
        queued.pop_front();
        return data;
    }
};

future<void> *allocateFunction();

class AsyncRunner
{
protected:
    ThreadSafeQueue<future<void> *> asyncs;

    // TODO: check asyncs running and terminate stopped

public:
    void queue(std::function<void()> function)
    {
        future<void> *execution = allocateFunction();
        *execution = async(launch::async, function);
        asyncs.queue(execution);
    }
};

time_t getAccessTime(std::string path);
time_t getCreateTime(std::string path);
time_t getModificationTime(std::string path);

time_t toTimeT(std::string value);
std::string toString(time_t value);
