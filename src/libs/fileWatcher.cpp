#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define BUF_LEN (1024 * (EVENT_SIZE + 16))

#include "fileWatcher.h"

void watch(
    std::string path,
    std::function<void(std::string)> onFileCreated,
    std::function<void(std::string)> onFileModified,
    std::function<void(std::string)> onFileDeleted)
{

    int inotifyId = inotify_init();

    if (inotifyId < 0)
    {
        std::cout << "Error while initializing inotifier." << std::endl;
        exit(-1);
    }

    int watchId = inotify_add_watch(inotifyId, path.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE | IN_MOVE_SELF);

    while (true)
    {
        char buffer[BUF_LEN];
        int length = read(inotifyId, buffer, BUF_LEN);
        if (length < 0)
        {
            std::cout << "Error while reading folder events." << std::endl;
            exit(-1);
        }

        int i = 0;
        while (i < length)
        {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];

            i += EVENT_SIZE + event->len;

            if (!event->len)
            {
                std::cout << "Empty event." << std::endl;
                continue;
            }

            if (event->mask & IN_ISDIR)
            {
                std::cout << "Folder events are to be ignored." << std::endl;
                continue;
            }

            std::string filename((char *)event->name);

            if (event->mask & IN_CREATE)
            {
                onFileCreated(filename);
            }

            if (event->mask & IN_DELETE)
            {
                onFileDeleted(filename);
            }

            if (event->mask & IN_MODIFY)
            {
                onFileModified(filename);
            }

            // For simplicity, I just delete and recreate files when moving.

            if (event->mask & IN_MOVED_FROM)
            {
                onFileDeleted(filename);
            }

            if (event->mask & IN_MOVED_TO)
            {
                onFileCreated(filename);
            }
        }
    }

    inotify_rm_watch(inotifyId, watchId);
    close(inotifyId);

    exit(0);
}
