#pragma once
#include <cstddef>
#include <cstdint>

namespace ZerOS::base {

namespace zeros_impl {
static constexpr std::size_t _ctz(std::uint32_t x) {
    std::size_t n = 0;
    while ((x & 1u) == 0) {
        x >>= 1;
        ++n;
    }
    return n;
}
} // namespace zeros_impl

/// Index of the lowest set bit; x must be non-zero.
/// GCC/Clang lower this to RBIT+CLZ on Cortex-M3/M4.
constexpr std::size_t ctz(std::uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<std::size_t>(__builtin_ctz(x));
#else
    return zeros_impl::_ctz(x);
#endif
} // ctz

/**
 * @brief  A bare-word bitmap for kernel bookkeeping (allocators, schedulers).
 *
 *         Contracts (break them and the helpers will lie to you):
 *         - Padding bits (>= bit_count in the tail word) must stay 0.
 *           word() hands out raw access on purpose: keeping the tail
 *           padding clean while writing whole words is on the caller.
 *         - set()/clear() are read-modify-write, NOT atomic. Guard them
 *           with a critical section / exclusive access when ISRs share the map.
 *         - Out-of-range bit/word indexes are UB.
 */
template <std::size_t bit_count> struct Bitmap {
    static_assert(bit_count > 0, "ZerOS::base::Bitmap needs at least one bit");

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    static constexpr std::size_t WORDS = (bit_count + 31) >> 5;

    // all-zero -> lands in .bss, zero flash cost, constinit friendly
    constexpr Bitmap() = default;
    constexpr Bitmap(Bitmap&&) noexcept = default;
    constexpr Bitmap& operator=(Bitmap&&) noexcept = default;

    // —— 位级 ——
    constexpr void set(std::size_t i) { words_[i >> 5] |= one_hot(i); }
    constexpr void clear(std::size_t i) { words_[i >> 5] &= ~one_hot(i); }
    [[nodiscard]] constexpr bool test(std::size_t i) const {
        return (words_[i >> 5] & one_hot(i)) != 0;
    }

    // —— 字级 (what std::bitset refuses to give us) ——
    /// Raw access to the w-th 32-bit plane: bulk set/clear, L1 summaries,
    /// word-wide atomics. Keep the tail padding zero!
    constexpr std::uint32_t& word(std::size_t w) { return words_[w]; }
    [[nodiscard]] constexpr std::uint32_t word(std::size_t w) const { return words_[w]; }

    /// All real bits occupied? Tail word compares against tail_mask(), NOT 0xFFFFFFFF!
    [[nodiscard]] constexpr bool word_full(std::size_t w) const {
        return words_[w] == valid_mask(w);
    }

    /// All 32 slots free? Safe for the tail word too, padding stays 0.
    [[nodiscard]] constexpr bool word_empty(std::size_t w) const { return words_[w] == 0; }

    // —— CLZ 查找 ——
    /// First clear bit inside the w-th word, npos if that word is full.
    /// The second CLZ step of a two-level lookup: level-1 finds the word,
    /// this lands the bit. Tail-word safe: padding never fakes a hit.
    [[nodiscard]] constexpr std::size_t first_zero_in_word(std::size_t w) const {
        const std::uint32_t free_bits = ~words_[w] & valid_mask(w);
        return free_bits != 0 ? (w << 5) + ctz(free_bits) : npos;
    }

    /// First clear bit (a free slot), npos if none.
    /// Skips whole words with one compare; RBIT+CLZ inside on Cortex-M3+.
    [[nodiscard]] constexpr std::size_t find_first_zero() const {
        for (std::size_t w = 0; w < WORDS; ++w) {
            const std::size_t bit = first_zero_in_word(w);
            if (bit != npos) {
                return bit;
            }
        }
        return npos;
    }

    /// First set bit, npos if none. Tail word is naturally safe: padding 0s never fake-hit.
    [[nodiscard]] constexpr std::size_t find_first_set() const {
        for (std::size_t w = 0; w < WORDS; ++w) {
            if (words_[w] != 0) {
                return (w << 5) + ctz(words_[w]);
            }
        }
        return npos;
    }

    // static_assert on it, memcpy it, summarize it.
    // so public it
    std::uint32_t words_[WORDS]{};

  private:
    // A Copy cast is not thought as popular, i think!
    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;

    static constexpr std::uint32_t one_hot(std::size_t i) { return 1u << (i & 31); }

    /// Valid-bit mask of the w-th word: full for a plain word, only
    /// the real bits for the tail word. Anchor of every tail-safe check.
    static constexpr std::uint32_t valid_mask(std::size_t w) {
        return (w + 1 == WORDS) ? tail_mask() : ~0u;
    }

    static constexpr std::uint32_t tail_mask() {
        // (bit_count & 31) == 0 would shift by 32, which is UB -> ~0u instead
        return (bit_count & 31) ? (1u << (bit_count & 31)) - 1u : ~0u;
    }
};

// —— Compile-time self checks: free unit tests, zero runtime cost ——
static_assert([] {
    Bitmap<8> b;
    b.set(0); b.set(1);
    return b.find_first_zero() == 2;
}());

static_assert([] {
    Bitmap<5> b;                          // tail word carries 3 padding bits
    for (std::size_t i = 0; i < 5; ++i) b.set(i);
    return b.find_first_zero() == Bitmap<5>::npos;  // padding must never fake a hit
}());

static_assert([] {
    Bitmap<8> b;
    b.set(7);
    return b.find_first_set() == 7 && b.test(7) && !b.test(0);
}());

static_assert(Bitmap<8>{}.find_first_set() == Bitmap<8>::npos);

} // namespace ZerOS::base
