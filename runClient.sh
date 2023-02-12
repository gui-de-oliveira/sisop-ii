#!/bin/bash

g++ -o build/client src/libs/socket.cpp src/libs/fileManager.cpp src/libs/helpers.cpp src/libs/message.cpp src/libs/fileWatcher.cpp src/client.cpp
cd in/$1
../../build/client $2 localhost $3