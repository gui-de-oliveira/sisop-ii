#include <future>
#include <list>
#include <unistd.h>

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
    void queue(std::future<void> running)
    {
        future<void> *execution = allocateFunction();
        execution = &running;
        asyncs.queue(execution);
    }
};