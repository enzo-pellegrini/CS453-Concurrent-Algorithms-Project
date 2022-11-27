//
// Created by Vincenzo Pellegrini on 27/11/22.
//

#include "SharedRegion.h"
#include "macros.hpp"

SharedRegion::SharedRegion(size_t size, size_t align) {
    alignment = align;
    virtualMemoryArray = new Segment*[65536];
    virtualMemoryLen = 0;
    virtualMemoryArray[virtualMemoryLen++] = new Segment(size, align);
    globalVersion = 0;
}

SharedRegion::~SharedRegion() {
    for (int i=0; i<virtualMemoryLen; i++) {
        if (virtualMemoryArray[i] != nullptr)
            delete virtualMemoryArray[i];
    }
    delete[] virtualMemoryArray;
}

size_t SharedRegion::getAlignment() const {
    return alignment;
}

size_t SharedRegion::getFirstSegmentSize() const {
    return virtualMemoryArray[0]->size;
}

void* SharedRegion::getFirstSegmentVa() {
    return va_from_index(0, 0);
}
