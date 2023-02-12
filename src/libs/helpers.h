#include <future>
#include <list>
#include <unistd.h>
#include <optional>
#include <functional>
#include <iostream>
#include <stdio.h>

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

    std::optional<T> pop()
    {
        if (queued.empty())
        {
            return std::nullopt;
        }

        T data = queued.front();
        queued.pop_front();
        return data;
    }
};

template <typename T>
class QueueProcessor
{
    ThreadSafeQueue<T> _queue;

    bool isExiting = false;

    void processQueue()
    {
        while (!isExiting)
        {
            auto result = _queue.pop();

            if (!result.has_value())
            {
                sleep(1);
                continue;
            }

            auto value = result.value();
            processEntry(value);
        }
    }

    std::future<void> processor;
    std::function<void(T)> processEntry;

public:
    QueueProcessor(std::function<void(T)> processEntry)
    {
        this->processEntry = processEntry;
        this->processor = std::async(
            launch::async,
            [this]
            { this->processQueue(); });
    }

    void stop()
    {
        this->isExiting = true;
        processor.get();
    }

    void queue(T request)
    {
        _queue.queue(request);
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
