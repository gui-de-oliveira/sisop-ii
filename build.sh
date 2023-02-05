#!/bin/bash

g++ -o server message.cpp socket.cpp server.cpp
g++ -o client message.cpp socket.cpp client.cpp
g++ -o foobar message.cpp socket.cpp foobar.cpp