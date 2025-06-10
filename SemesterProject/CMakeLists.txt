cmake_minimum_required(VERSION 3.10)
project(GTU_OS_Simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
add_executable(simulate
    main.cpp
    CPU.cpp
) 