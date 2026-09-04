
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>

#include "ZerOS/base/bitmap.hpp"
#include "ZerOS/kernel/mem/pool.hpp"

namespace ZerOS::memory {

#define ALL_ALIGNED alignas(alignof(std::max_align_t))

/**
 * @brief   This is a bitmap pool, allocating the stuff of buffer_
 *          Take a breathe, we use bitmap, which, we use a bit to shoow if we use a block
 *          Mentioned: One Block, One Stuff
 *
 * @tparam block_size
 * @tparam block_cnt
 * @tparam owns_poison_policy
 */
template <std::size_t block_size, std::size_t block_cnt, bool owns_poison_policy>
struct BitmapPool {
    static constexpr std::size_t BUFFER_SIZE = block_size * block_cnt;
    static constexpr std::size_t BITMAP_L2_SIZE = (block_cnt + 31) / 32;
    static constexpr std::size_t BLOCK_SIZE = block_size;

    static_assert(BLOCK_SIZE % alignof(std::max_align_t) == 0,
                  "block size must keep every block max-aligned");
    // Why not optional
    // A. if we use optional, for most impls, it costs 8 bytes
    // B. for User Interfaces, we should never carry it

    constexpr BitmapPool() = default;

    // ------------------------------------------------------------------
    // Contract surface: these three together satisfy concept MemoryPool
    // ------------------------------------------------------------------
    std::expected<void*, MemoryAllocationError> raw_allocate() {
        const auto index = available_block_index();
        if (!IsAvailableIndex(index)) {
            return std::unexpected(MemoryAllocationError::OutOfMemory);
        }

        if constexpr (owns_poison_policy) {
            // Only blocks that HAVE been poisoned (freed once) can fail the check;
            // fresh .bss blocks are all-zero, which is not poison tampering.
            if (ever_poisoned_.test(index) && detected_poison(index)) {
                return std::unexpected(MemoryAllocationError::Poisoned);
            }
        }

        set_as_in_used(index);
        return fetch_target_block(index);
    }
    MemoryAllocationError raw_deallocate(void* block) {
        const auto index = index_of_given_ptr(block);
        if (!IsAvailableIndex(index)) {
            return MemoryAllocationError::NotOwned; // wild / interior / foreign pointer
        }

        if (!release_block(index)) {
            return MemoryAllocationError::NotOwned; // double free
        }

        poison_block(index); // no-op when poison policy is off
        return MemoryAllocationError::Ok;
    }

    // For ISR allocations: no expected, failure simply reported as nullptr
    void* try_allocate() {
        auto res = raw_allocate();
        return res ? *res : nullptr;
    }

  private:
    // we fetch the first available block, if not, return the
    // npos
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    static constexpr bool IsAvailableIndex(std::size_t index) { return index != npos; }

    // bitmap using here, for 0, it is available, for 1, it is full
    std::size_t available_block_index() // fetch the available block recorded in the pool
    {
        // Boost the speed by using the l1 bitmap, fastly, we find the first zero in the l1 bitmap
        const auto word_index = bitmap_l1_.find_first_zero();
        if (word_index == decltype(bitmap_l1_)::npos) {
            return npos;
        }

        // OK, this is the case, find in this word
        return bitmap_l2_.first_zero_in_word(word_index);
    }

    void set_as_in_used(std::size_t index) {
        bitmap_l2_.set(index);

        if (bitmap_l2_.word_full(index >> 5)) {
            bitmap_l1_.set(index >> 5); // ok, this is also full
        }

        used_++;
    }

    bool release_block(std::size_t index) // The index acquired by available_block_index
    {
        if (index >= block_cnt) {
            return false;
        }

        if (!bitmap_l2_.test(index)) {
            // you cant release a block that is not in use
            return false;
        }

        bitmap_l2_.clear(index);
        // l1 is the "word full" flag: freeing ANY block makes its word not-full.
        // Unconditional clear is idempotent and keeps the invariant honest.
        bitmap_l1_.clear(index >> 5);

        used_--;
        return true;
    }

    constexpr void* fetch_target_block(std::size_t index) { return buffer_ + index * BLOCK_SIZE; }
    std::size_t index_of_given_ptr(void* ptr) {
        auto* p = static_cast<std::byte*>(ptr);
        if (p < buffer_ || p >= buffer_ + BUFFER_SIZE) {
            // Not in this scpoe
            return npos;
        }

        const auto off = static_cast<std::size_t>(p - buffer_);
        if (off % BLOCK_SIZE != 0) {
            // not aligned, All the target allocated should be aligned
            return npos;
        }

        return off / BLOCK_SIZE;
    }

    // poisoned the target block
    static constexpr std::byte POISON_VALUE{0x67};
    void poison_block(std::size_t index) {
        if constexpr (owns_poison_policy) {
            auto* p = fetch_target_block(index);
            memset(p, std::to_integer<int>(POISON_VALUE), BLOCK_SIZE);
            ever_poisoned_.set(index); // from now on, this block is expected to stay poisoned
        }
    }
    bool detected_poison(std::size_t index) {
        if constexpr (!owns_poison_policy) {
            return false;
        }
        auto* p = static_cast<std::byte*>(fetch_target_block(index));
        for (std::size_t i = 0; i < BLOCK_SIZE; ++i) {
            if (p[i] != POISON_VALUE) {
                return true; // One write the session!
            }
        }
        return false;
    }

    // Buffer Locations here, as it request all baasic
    ALL_ALIGNED std::byte buffer_[BUFFER_SIZE];
    base::Bitmap<block_cnt> bitmap_l2_;      // one bit per block: 1 = occupied
    base::Bitmap<BITMAP_L2_SIZE> bitmap_l1_; // one bit per l2 word: 1 = that word is full
    base::Bitmap<block_cnt> ever_poisoned_;  // 1 = block went through a poison-on-free cycle
    std::size_t used_ = 0; // block we have been used
};

#undef ALL_ALIGNED // OK, dont leek this out

// we should ensure that, BitmapPool is A Memory Pool
static_assert(MemoryPool<BitmapPool<64, 8, true>>);

} // namespace ZerOS::memory
