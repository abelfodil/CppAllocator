cmake_minimum_required(VERSION 3.28)
project(CppAllocator)

set(CMAKE_CXX_STANDARD 23)

include_directories(.)

find_package(TBB REQUIRED COMPONENTS tbb)

add_executable(CppAllocator
        main.cpp)

target_link_libraries(CppAllocator tbb)