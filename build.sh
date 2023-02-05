#!/bin/bash

g++ -o build/server src/libs/socket.cpp src/libs/fileManager.cpp src/libs/helpers.cpp src/libs/message.cpp src/server.cpp
g++ -o build/client src/libs/socket.cpp src/libs/fileManager.cpp src/libs/helpers.cpp src/libs/message.cpp src/client.cpp
# g++ -o build/foobar src/libs/socket.cpp src/libs/fileManager.cpp src/libs/helpers.cpp src/libs/message.cpp src/sandbox/foobar.cpp