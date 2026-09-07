cmake_minimum_required(VERSION 4.3)
project(playoffpredictor)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

add_executable(playoffpredictor main.cpp
        qm.cpp
        qm.h
        ts.cpp
        ts.h
        cs.cpp
        cs.h)
