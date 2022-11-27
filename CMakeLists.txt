cmake_minimum_required(VERSION 3.23)
project(CS453_Concurrent_Algorithms_Project)

set(CMAKE_CXX_STANDARD 14)

include_directories(353607)
include_directories(grading)
include_directories(include)

add_executable(CS453_Concurrent_Algorithms_Project
        353607/macros.h
        353607/tm.c
        353607/versioned_lock.c
        353607/versioned_lock.h
        grading/common.hpp
        grading/grading.cpp
        grading/transactional.hpp
        grading/workload.hpp
        include/tm.h
        include/tm.hpp)
