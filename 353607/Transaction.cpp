//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#include "Transaction.h"

#include <cstddef>
#include <iostream>

Transaction::Transaction(bool isRo) : isRo{isRo} {
    rv = 0;
    if (isRo) {
        // Should leave readset and writeset unitialized but not necessarly easy
    }
}

Transaction::~Transaction() {
    for (auto &i: writeSet) {
        delete i.second;
    }
}

void Transaction::begin(SharedRegion *sr) {
    rv = sr->globalVersion;
//    std::cout << "Transaction started, rv is " << rv << std::endl;
//    sr->cleanupLock.lock_shared();
}

bool Transaction::commit(SharedRegion *sr) {
    if (isRo) {
//        std::cout << "Committed read only transaction" << std::endl;
        return true;
    }

//    std::cout << "Started a commit " << readSet.size() << " " << writeSet.size() << std::endl;

    // Lock the write-set
    int numLocked = 0;
    bool shouldAbort = false;
    for (auto &i: writeSet) {
        Segment *segment = sr->virtualMemoryArray[index_from_va(i.second->virtualAddress)];
        std::atomic<int> *lock = &segment->locks[offset_from_va(i.second->virtualAddress) / sr->alignment];
        int read = std::atomic_load(lock);
        if (read & 0x1 || (read >> 1) > rv) {
            shouldAbort = true;
            break;
        }
        if (!std::atomic_compare_exchange_strong(lock, &read, read | 0x1)) {
            shouldAbort = true;
            break;
        }
//        std::cout << "locked lock, wrote " << *lock << ", va: " << (uintptr_t)i.second->virtualAddress << std::endl;
        numLocked++;
    }

    if (shouldAbort) {
        int count = 0;
        for (auto i = writeSet.begin(), e = writeSet.end(); i != e && count < numLocked; i++, count++) {
            Segment *segment = sr->virtualMemoryArray[index_from_va(i->second->virtualAddress)];
            std::atomic<int> *lock = &segment->locks[offset_from_va(i->second->virtualAddress) / sr->alignment];
            std::atomic_fetch_sub(lock, 1);

//            std::cout << "unlocked lock, wrote " << *lock << ", va: " << (uintptr_t)i->second->virtualAddress << std::endl;
        }

        return false;
    }

    // Increment global version lock
    int wv = std::atomic_fetch_add(&sr->globalVersion, 1) + 1;

    // Validate the read-set
    for (std::atomic<int> *lock: readSet) {
        int read = std::atomic_load(lock);
        if (read & 0x1 || (read >> 1) > rv) {
            for (auto i = writeSet.begin(), e = writeSet.end(); i != e; i++) {
                Segment *segment = sr->virtualMemoryArray[index_from_va(i->second->virtualAddress)];
                std::atomic<int> *locked = &segment->locks[offset_from_va(i->second->virtualAddress) / sr->alignment];
                std::atomic_fetch_sub(locked, 1);
            }

            return false;
        }
    }

    // Write to memory and unlock write-set
    for (auto &i: writeSet) {
        Segment *segment = sr->virtualMemoryArray[index_from_va(i.second->virtualAddress)];
        std::atomic<int> *lock = &segment->locks[offset_from_va(i.second->virtualAddress) / sr->alignment];
        memcpy((void *) ((uintptr_t) segment->data + offset_from_va(i.second->virtualAddress)),
               i.second->value, sr->alignment);
        std::atomic_store(lock, (wv << 1));
//        std::cout << "wrote " << *lock << " to lock" << std::endl;
    }

//    std::cout << "commited a transaction that wrote to " << writeSet.size() << std::endl;
//    sr->cleanupLock.unlock_shared();
    return true;
}

bool Transaction::read(SharedRegion *sr, void *source, size_t size, void *target) {
    int startIndex = offset_from_va(source) / sr->alignment;
    int numWords = size / sr->alignment;
    Segment *s = sr->virtualMemoryArray[index_from_va(source)];

    if (isRo) {
        for (int offset = 0; offset < numWords; offset++) {
            memcpy((void *) ((uintptr_t) target + offset * sr->alignment),
                   (void *) ((uintptr_t) s->data + (startIndex + offset) * sr->alignment), sr->alignment);
            // Post validate by checking the version lock
            int read = s->locks[startIndex + offset];
            if ((read & 0x1) > 0 || (read >> 1) > rv) {
//                std::cout << "Failing ro read because read post-validation failed, read " << read << " va: "
//                          << (uintptr_t) source + offset * sr->alignment << std::endl;
                return false;
            }
        }
    } else {
        for (int offset = 0; offset < numWords; offset++) {
            // Check if in write set
            if (auto res = writeSet.find((void *) ((uintptr_t) source + offset * sr->alignment)); res !=
                                                                                                  writeSet.end()) {
                memcpy((void *) ((uintptr_t) target + offset * sr->alignment), res->second->value, sr->alignment);

                continue;
            }

            // Otherwise read from memory
            std::atomic<int> *lock = &s->locks[startIndex + offset];
            int read = std::atomic_load(lock);
            if ((read & 0x1) > 0 || (read >> 1) > rv) {
                return false;
            }
            memcpy((void *) ((uintptr_t) target + offset * sr->alignment),
                   (void *) ((uintptr_t) s->data + (startIndex + offset) * sr->alignment), sr->alignment);
            int nRead = atomic_load(lock);
            if (nRead != read) {
//                std::cout << "Failing read because read post-validation failed, read " << nRead << ", expected: " << read
//                          << " va: " << (uintptr_t) source + offset * sr->alignment << std::endl;
                return false;
            }

            // And add to read-set
            readSet.insert(&s->locks[offset + startIndex]); // Does this add duplicates?
        }
    }

    return true;
}

bool Transaction::write(SharedRegion *sr, const void *source, size_t size, void *target) {
    if (isRo)
        return false;
    Segment *s = sr->virtualMemoryArray[index_from_va(target)];
    const int startIndex = offset_from_va(target) / sr->alignment;
    int numWords = size / sr->alignment;

    for (int offset = 0; offset < numWords; offset++) {
        const void *currVa = (void *) ((uintptr_t) target + offset * sr->alignment);
        // check if in write set
        auto wsRes = writeSet.find(currVa);
        if (wsRes != writeSet.end()) {
            memcpy(wsRes->second->value, (void *) ((uintptr_t) source + offset * sr->alignment), sr->alignment);
            continue;
        }

        // add word to write-set
        void *tmp = malloc(sr->alignment);
        memcpy(tmp, (void *) ((uintptr_t) source + offset * sr->alignment), sr->alignment);
        auto wi = new WriteItem(currVa, tmp);
        writeSet.insert(std::pair{currVa, wi});
        // remove from readset
        std::atomic<int> *lock = &s->locks[startIndex + offset];
        readSet.erase(lock);
    }

    return true;
}

Alloc Transaction::alloc(SharedRegion *sr, size_t size, void **target) {
    // std::cout << "Called tm_alloc\n";

    sr->virtualMemoryLock.lock();

    int spot;
    if (!sr->emptySpots.empty()) {
        spot = sr->emptySpots.front();
        sr->emptySpots.pop_front();
    } else {
        spot = sr->virtualMemoryLen++;
    }

    sr->virtualMemoryLock.unlock();

    sr->virtualMemoryArray[spot] = new Segment(size, sr->alignment);

    allocated.push_back(spot);

    *target = va_from_index(spot, 0);

    return Alloc::success;
}

bool Transaction::dealloc(SharedRegion *sr, void *target) {
    // TODO: implement
    return true;
}

Transaction::WriteItem::WriteItem(const void *virtualAddress, void *value)
        : virtualAddress(virtualAddress), value(value) {}

Transaction::WriteItem::~WriteItem() {
    free(value);
}
