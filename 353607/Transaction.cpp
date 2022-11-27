//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#include "Transaction.h"

Transaction::Transaction(bool isRo) : isRo{isRo} {
    rv = 0;
    if (isRo) {
        // Should leave readset and writeset unitialized but not necessarly easy
    }
}

void Transaction::begin(SharedRegion *sr) {
    rv = sr->globalVersion;
    sr->cleanupLock.lock_shared();
}

bool Transaction::commit(SharedRegion *sr) {
    sr->cleanupLock.unlock_shared();

    return true;
}

bool Transaction::read(SharedRegion *sr, void *source, size_t size, void *target) {
    int startIndex = offset_from_va(source) / sr->getAlignment();
    int numWords = size / sr->getAlignment();
    Segment *s = sr->virtualMemoryArray[index_from_va(source)];

    if (isRo) {
        for (int i = startIndex; i < startIndex + numWords; i++) {
            memcpy((void *) ((uintptr_t) target + (i - startIndex) * sr->alignment),
                   (void *) ((uintptr_t) source + (i - startIndex) * sr->alignment), sr->alignment);
            // Post validate by checking the version lock
            int read = s->locks[i];
            if ((read & 0x1) > 0 || (read >> 1) > rv) {
                return false;
            }
        }
    } else {
        for (int offset=0; offset < numWords; offset++) {
            // Check if in write set
            if (auto res = writeSet.find((void*)((uintptr_t)source + offset * sr->alignment)); res != writeSet.end()) {
                memcpy((void*)((uintptr_t)target + offset*sr->alignment), res->second.value, sr->alignment);

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
            readSet.insert(&s->locks[offset + startIndex]); // Does this add duplicaes
        }
    }

    return true;
}

Transaction::WriteItem::WriteItem(void *virtualAddress, void *value, void *rawAddr, std::atomic_int *versionedLock)
        : virtualAddress(virtualAddress), value(value), rawAddr(rawAddr), versionedLock(versionedLock) {}

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
