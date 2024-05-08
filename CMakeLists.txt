cmake_minimum_required(VERSION 3.28)
project(CppAllocator)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(.)

find_package(TBB REQUIRED COMPONENTS tbb)

add_executable(CppAllocator
        main.cpp)

target_link_libraries(CppAllocator tbb)

include(FetchContent)

###########
## Tests ##
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
        CppAllocator_test
        main_test.cpp
)
target_link_libraries(
        CppAllocator_test
        GTest::gtest_main
        tbb
)

include(GoogleTest)
gtest_discover_tests(CppAllocator_test)

###############
## Benchmark ##
set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON)

FetchContent_Declare(
        benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
FetchContent_MakeAvailable(benchmark)

add_executable(
        CppAllocator_benchmark
        main_benchmark.cpp
)
target_link_libraries(
        CppAllocator_benchmark
        benchmark::benchmark
        tbb
)
