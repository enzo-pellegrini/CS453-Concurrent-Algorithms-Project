//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#ifndef CS453_CONCURRENT_ALGORITHMS_PROJECT_TRANSACTION_H
#define CS453_CONCURRENT_ALGORITHMS_PROJECT_TRANSACTION_H

#include <tm.hpp>
#include "SharedRegion.h"

#include <unordered_set>
#include <map>
#include <atomic>
#include <ostream>

class Transaction {
public:
    explicit Transaction(bool isRo);
    ~Transaction();
    void begin(SharedRegion* sr);
    bool commit(SharedRegion* sr);
    bool read(SharedRegion* sr, void* source, size_t size, void* target);
    bool write(SharedRegion* sr, const void* source, size_t size, void* target);
    Alloc alloc(SharedRegion* sr, size_t size, void** target);
    bool dealloc(SharedRegion* sr, void* target);
private:
    bool isRo;
    int rv;

    std::unordered_set<std::atomic<int>*> readSet;

    class WriteItem {
    public:
        const void* virtualAddress;
        void* value;

        WriteItem(const void *virtualAddress, void *value);
        ~WriteItem();
    };

    std::map<const void*, WriteItem*> writeSet;
    std::vector<int> allocated; // vector of indexes of allocated segments
};


#endif //CS453_CONCURRENT_ALGORITHMS_PROJECT_TRANSACTION_H
