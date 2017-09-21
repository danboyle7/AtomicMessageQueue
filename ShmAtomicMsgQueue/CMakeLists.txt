cmake_minimum_required(VERSION 3.6)
project(mqutils)


#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -std=c++11")

set(SOURCE_FILES MessageQueue.h shmIPC.h events.h)

add_library(HEADER_ONLY_TARGET STATIC MessageQueue.h shmIPC.h events.h)
set_target_properties(HEADER_ONLY_TARGET PROPERTIES LINKER_LANGUAGE CXX)

INSTALL(FILES
        MessageQueue.h
        DESTINATION "/usr/local/include"
        )

# set the path to the library folder
link_directories(/usr/local/include/boost)
link_directories(/usr/local/lib)
