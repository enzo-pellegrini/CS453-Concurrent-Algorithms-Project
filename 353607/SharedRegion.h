//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#ifndef CS453_CONCURRENT_ALGORITHMS_PROJECT_SHAREDREGION_H
#define CS453_CONCURRENT_ALGORITHMS_PROJECT_SHAREDREGION_H


#include <cstddef>
#include <atomic>
#include <shared_mutex>
#include <vector>
#include <list>
#include "macros.hpp"

struct Segment {
    size_t size;
    std::atomic<int> *locks;
    void* data;

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cppcoreguidelines-pro-type-member-init"
    Segment(size_t size, size_t align) : size{size} {
        int err = posix_memalign(&data, align, size);
        locks = new std::atomic<int>[size/align];
    }
#pragma clang diagnostic pop

    ~Segment() {
        free(data);
        delete[] locks;
    }
};

class SharedRegion {
public:
    SharedRegion(size_t size, size_t align);
    ~SharedRegion();
    size_t getAlignment() const;
    size_t getFirstSegmentSize();
    static void* getFirstSegmentVa();

    std::atomic<int> globalVersion{};
    size_t alignment;
    std::shared_mutex cleanupLock;
    Segment** virtualMemoryArray;
private:
    std::mutex virtualMemoryLock;
    int virtualMemoryLen;
    std::list<int> emptySpots;
};


#endif //CS453_CONCURRENT_ALGORITHMS_PROJECT_SHAREDREGION_H
