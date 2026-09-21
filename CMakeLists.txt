cmake_minimum_required(VERSION 4.3)
project(playoffpredictor CXX CUDA)

set(CMAKE_CUDA_COMPILER "C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v13.3\\bin\\nvcc.exe")

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CUDA_STANDARD 20)

set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --expt-relaxed-constexpr")

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

add_executable(playoffpredictor main.cpp
        qm.cpp
        qm.h
        ts.cpp
        ts.h
        cuda.cu
        cuda.cuh
)

set_target_properties(
        playoffpredictor
        PROPERTIES
        CUDA_SEPERABLE_COMPILATION ON
)