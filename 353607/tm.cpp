
// Internal headers
#include <tm.hpp>

#include "macros.hpp"

shared_t tm_create(size_t, size_t) noexcept;
void     tm_destroy(shared_t) noexcept;
void*    tm_start(shared_t) noexcept;
size_t   tm_size(shared_t) noexcept;
size_t   tm_align(shared_t) noexcept;
tx_t     tm_begin(shared_t, bool) noexcept;
bool     tm_end(shared_t, tx_t) noexcept;
bool     tm_read(shared_t, tx_t, void const*, size_t, void*) noexcept;
bool     tm_write(shared_t, tx_t, void const*, size_t, void*) noexcept;
Alloc    tm_alloc(shared_t, tx_t, size_t, void**) noexcept;
bool     tm_free(shared_t, tx_t, void*) noexcept;
