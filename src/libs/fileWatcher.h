#include <functional>
#include <string>

void watch(
    std::string path,
    std::function<void(std::string)> onFileCreated,
    std::function<void(std::string)> onFileModified,
    std::function<void(std::string)> onFileDeleted);