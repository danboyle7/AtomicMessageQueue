#!/bin/bash

# Remove other message queue library files that wont be overwritten
sudo rm -rf /usr/local/libmqutils.a

# Add message queue headers to /usr/local/include
sudo cp ./shmIPC.h /usr/local/include
sudo cp ./MessageQueue.h /usr/local/include
sudo cp ./events.h /usr/local/include
sudo cp ./events2.h /usr/local/include
