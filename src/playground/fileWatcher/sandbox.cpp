#include <iostream>
#include <functional>
using namespace std;

#include "../../libs/fileWatcher.h"

int main()
{
    watch(
        "folder",
        [](std::string filename)
        { std::cout << "File " << filename << " created." << std::endl; },

        [](std::string filename)
        { std::cout << "File " << filename << " modified." << std::endl; },

        [](std::string filename)
        { std::cout << "File " << filename << " deleted." << std::endl; });
}