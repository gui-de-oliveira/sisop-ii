#include "helpers.h"
#include <future>

using namespace std;

future<void> *allocateFunction()
{
    return (future<void> *)malloc(sizeof(future<void>));
}