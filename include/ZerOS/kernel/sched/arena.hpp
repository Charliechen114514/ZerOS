#pragma once

// Startup-only bump arena: slices fly off the block at wiring time; once
// the scheduler starts, the block is frozen. No heap, no free, no
// fragmentation — "allocation" is a pointer bump. Running dry is a wiring
// bug, the caller decides how loud to die.

#include <cstddef>
#include <cstdint>

namespace ZerOS::task {

class Arena {
  public:
    constexpr Arena() = default;
    constexpr Arena(std::byte* block, std::size_t bytes) noexcept
        : cursor_{block}, left_{bytes} {}

    // alignment-rounded slice, or nullptr when the block ran dry
    [[nodiscard]] void* take(std::size_t bytes, std::size_t align) noexcept {
        const auto base = reinterpret_cast<std::uintptr_t>(cursor_);
        const auto miss = (align - (base % align)) % align;
        if (miss + bytes > left_) {
            return nullptr;
        }
        cursor_ += miss;
        left_ -= miss;
        auto* out = cursor_;
        cursor_ += bytes;
        left_ -= bytes;
        return out;
    }

    [[nodiscard]] std::size_t left() const noexcept { return left_; }

  private:
    std::byte* cursor_{};
    std::size_t left_{};
};

} // namespace ZerOS::task
