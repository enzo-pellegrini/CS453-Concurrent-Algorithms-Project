cmake_minimum_required(VERSION 3.23)
project(CS453_Concurrent_Algorithms_Project)

set(CMAKE_CXX_STANDARD 17)

include_directories(grading)
include_directories(include)
include_directories(template)

add_executable(CS453_Concurrent_Algorithms_Project
        grading/common.hpp
        grading/grading.cpp
        grading/transactional.hpp
        grading/workload.hpp
        include/tm.h
        include/tm.hpp
        353607/tm.cpp 353607/SharedRegion.cpp 353607/SharedRegion.h 353607/Transaction.cpp playground/playground.cpp playground/main.h)
