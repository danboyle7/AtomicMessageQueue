cmake_minimum_required(VERSION 3.6)
project(ShmAtomicMsgQueue)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES shmIPC_impl.h shmIPC.h main.cpp MessageQueue.h pevents.h events.h)
add_library(ShmAtomicMsgQueue ${SOURCE_FILES})