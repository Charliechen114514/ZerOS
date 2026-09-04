#pragma once
#include <concepts>
#include <expected>

namespace ZerOS::memory
{
    enum class MemoryAllocationError {
        Ok, OutOfMemory, Poisoned, NotOwned
    };

    template <typename Pool>
    concept MemoryPool = requires (Pool p, void* block) {
        // A Memory pool should be able to allocate and deallocate blocks
        {p.raw_allocate()} -> std::same_as<std::expected<void*, MemoryAllocationError>>;
        // And also can deallocate target
        {p.raw_deallocate(block)} -> std::same_as<MemoryAllocationError>;

        // for ISR Allocations, we should use try allocate
        {p.try_allocate()} -> std::same_as<void*>;
    };
}
