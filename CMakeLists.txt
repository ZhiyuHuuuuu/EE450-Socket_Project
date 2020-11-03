cmake_minimum_required(VERSION 3.15)
project(sokcet_project)

set(CMAKE_CXX_STANDARD 14)

add_executable(sokcet_project
               servermain.cpp
               serverA.cpp
               serverB.cpp
        client.cpp
        )