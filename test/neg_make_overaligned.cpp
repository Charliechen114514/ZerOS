// Negative compile test: this translation unit MUST fail to build.
//
// Make<> has to reject, at compile time, objects whose alignment exceeds
// the pool block's alignment (see BLOCK_ALIGN and typeable.hpp). The CMake
// side compiles this file with try_compile and requires FAILURE -- the
// only acceptable error source is the static_assert in Make.
//
// sizeof stays within BLOCK_SIZE on purpose, so the failure can only come
// from the alignof guard, never from the sizeof guard.
#include <cstddef>

#include "ZerOS/kernel/mem/bitmap_allocate.hpp"
#include "ZerOS/kernel/mem/typeable.hpp"

struct OverAligned {
    // One notch past max_align_t: perfectly legal C++, impossible to place
    // safely in a max-aligned pool block (unaligned placement new == UB).
    alignas(2 * alignof(std::max_align_t)) std::byte blob[64];
};

static_assert(sizeof(OverAligned) <= 64); // size guard alone must NOT trip

int main() {
    ZerOS::memory::BitmapPool<64, 2, false> pool;
    auto r = ZerOS::memory::Make<OverAligned>(pool);
    (void)r;
    return 0;
}
