//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#include "Transaction.h"

#include <cstddef>

Transaction::Transaction(bool isRo) : isRo{isRo} {
    rv = 0;
    if (isRo) {
        // Should leave readset and writeset unitialized but not necessarly easy
    }
}

void Transaction::begin(SharedRegion *sr) {
    rv = sr->globalVersion;
//    sr->cleanupLock.lock_shared();
}

bool Transaction::commit(SharedRegion *sr) {
    if (isRo)
        return true;



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
            int read = s->locks[startIndex + offset];
            if ((read & 0x1) > 0 || (read >> 1) > rv) {
                return false;
            }
            memcpy((void *) ((uintptr_t) target + offset * sr->alignment),
                   (void *) ((uintptr_t) s->data + (startIndex + offset) * sr->alignment), sr->alignment);
            if (s->locks[startIndex + offset] != read) {
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
    Segment *s = sr->virtualMemoryArray[index_from_va(source)];
    int startIndex = offset_from_va(source);
    int numWords = size / sr->alignment;

    for (int offset = 0; offset < numWords; offset++) {
        const void *currVa = (void *) ((uintptr_t) target + offset * sr->alignment);
        // check if in write set
        auto wsRes = writeSet.find(currVa);
        if (wsRes != writeSet.end()) {
            memcpy(wsRes->second->value, (void *) ((uintptr_t) source + offset * sr->alignment), sr->alignment);
            continue;
        }

        // add word to writeset
        void *tmp = malloc(sr->alignment);
        memcpy(tmp, (void *) ((uintptr_t) source + offset * sr->alignment), sr->alignment);
        auto wi = new WriteItem(currVa, tmp);
        writeSet.insert(std::pair{currVa, wi});
    }

    return true;
}

Alloc Transaction::alloc(SharedRegion *sr, size_t size, void **target) {
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

bool Transaction::WriteItem::operator==(const Transaction::WriteItem &rhs) const {
    return virtualAddress == rhs.virtualAddress;
}

bool Transaction::WriteItem::operator!=(const Transaction::WriteItem &rhs) const {
    return !(rhs == *this);
}

bool Transaction::WriteItem::operator<(const Transaction::WriteItem &rhs) const {
    return virtualAddress < rhs.virtualAddress;
}

bool Transaction::WriteItem::operator>(const Transaction::WriteItem &rhs) const {
    return rhs < *this;
}

bool Transaction::WriteItem::operator<=(const Transaction::WriteItem &rhs) const {
    return !(rhs < *this);
}

bool Transaction::WriteItem::operator>=(const Transaction::WriteItem &rhs) const {
    return !(*this < rhs);
}
