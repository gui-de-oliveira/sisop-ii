#!/bin/bash

g++ -o build/client \
 src/libs/client/fileState.cpp \
 src/libs/client/userCommands.cpp \
 src/libs/client/fileWatcher.cpp \
 src/libs/common/socket.cpp \
 src/libs/common/helpers.cpp \
 src/libs/common/message.cpp \
 src/client.cpp

cd in/$1
../../build/client $2 localhost $3