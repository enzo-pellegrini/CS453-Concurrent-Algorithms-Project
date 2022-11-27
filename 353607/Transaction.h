//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#ifndef CS453_CONCURRENT_ALGORITHMS_PROJECT_TRANSACTION_H
#define CS453_CONCURRENT_ALGORITHMS_PROJECT_TRANSACTION_H

#include <tm.hpp>
#include "SharedRegion.h"

#include <set>
#include <map>
#include <atomic>
#include <ostream>

class Transaction {
public:
    Transaction(bool isRo);
    void begin(SharedRegion* sr);
    bool commit(SharedRegion* sr);
    bool read(SharedRegion* sr, const void* source, size_t size, void* target);
    bool write(SharedRegion* sr, const void* source, size_t size, void* target);
    Alloc alloc(SharedRegion* sr, size_t size, void** target);
    bool dealloc(SharedRegion* sr, void* target);
private:
    bool isRo;
    int rv;

    std::set<std::atomic_int*> readSet;

    class WriteItem {
    public:
        void* virtualAddress;
        void* value;
        void* rawAddr;
        std::atomic_int* versionedLock;

        WriteItem(void *virtualAddress, void *value, void *rawAddr, std::atomic_int *versionedLock);

        bool operator==(const WriteItem &rhs) const;

        bool operator!=(const WriteItem &rhs) const;

        bool operator<(const WriteItem &rhs) const;

        bool operator>(const WriteItem &rhs) const;

        bool operator<=(const WriteItem &rhs) const;

        bool operator>=(const WriteItem &rhs) const;

        // TODO: generate hash thing
    };

    std::map<void*, WriteItem> writeSet;
};


#endif //CS453_CONCURRENT_ALGORITHMS_PROJECT_TRANSACTION_H