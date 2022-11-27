#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-use-auto"

// Internal headers
#include <tm.hpp>
#include "SharedRegion.h"
#include "Transaction.h"

shared_t tm_create(size_t size, size_t align) noexcept {
    SharedRegion* shared = new SharedRegion(size, align);
    return shared;
}

void     tm_destroy(shared_t sr) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    delete shared;
}

void*    tm_start(shared_t sr) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    return shared->getFirstSegmentVa();
}

size_t   tm_size(shared_t sr) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    return shared->getFirstSegmentSize();
}

size_t   tm_align(shared_t sr) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    return shared->getAlignment();
}

tx_t     tm_begin(shared_t sr, bool is_ro) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    Transaction* transaction = new Transaction(is_ro);
    transaction->begin(shared);
    return reinterpret_cast<tx_t>(transaction);
}

bool     tm_end(shared_t sr, tx_t tx) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    Transaction* transaction = reinterpret_cast<Transaction *>(tx);
    bool success = transaction->commit(shared);
    delete transaction;
    return success;
}

bool     tm_read(shared_t sr, tx_t tx, void const* source, size_t size, void* target) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    Transaction* transaction = reinterpret_cast<Transaction *>(tx);
    if (transaction->read(shared, source, size, target)) {
        return true;
    } else {
        delete transaction;
        return false;
    }
}

bool     tm_write(shared_t sr, tx_t tx, void const* source, size_t size, void* target) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    Transaction* transaction = reinterpret_cast<Transaction *>(tx);
    if (transaction->write(shared, source, size, target)) {
        return true;
    } else {
        delete transaction;
        return false;
    }
}

Alloc    tm_alloc(shared_t sr, tx_t tx, size_t size, void** target) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    Transaction* transaction = reinterpret_cast<Transaction *>(tx);
    Alloc out = transaction->alloc(shared, size, target);
    if (out != Alloc::abort) {
        return out;
    } else {
        delete transaction;
        return out;
    }
}

bool     tm_free(shared_t sr, tx_t tx, void* target) noexcept {
    SharedRegion* shared = static_cast<SharedRegion *>(sr);
    Transaction* transaction = reinterpret_cast<Transaction *>(tx);
    if (transaction->dealloc(shared, target)) {
        return true;
    } else {
        delete transaction;
        return false;
    }
}

#pragma clang diagnostic pop