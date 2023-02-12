#!/bin/bash
# chmod +x run.sh

mkdir build
mkdir build/folder
g++ -o build/sandbox ../../libs/fileWatcher.cpp sandbox.cpp && ./build/sandbox