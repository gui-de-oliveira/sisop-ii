#!/bin/bash

g++ -o build/server src/libs/socket.cpp src/libs/fileManager.cpp src/libs/helpers.cpp src/libs/message.cpp src/server.cpp
cd in/server
../../build/server $1