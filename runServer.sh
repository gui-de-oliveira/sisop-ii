#!/bin/bash

g++ -o build/server \
src/libs/common/socket.cpp \
 src/libs/common/message.cpp \
 src/libs/common/helpers.cpp \
 src/libs/server/fileManager.cpp \
 src/server.cpp

cd in/server
../../build/server $1