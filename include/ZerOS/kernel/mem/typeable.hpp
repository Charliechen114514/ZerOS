/**
 * @file typeable.hpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief typable facaded
 * @version 0.1
 * @date 2026-09-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#pragma once

#include "pool.hpp"
#include <expected>
#include <memory>

namespace ZerOS::memory
{
    // ObjectType first: it never appears in the parameter list, so callers
    // write Make<T>(pool, args...) and let the pool type be deduced.
    template<typename ObjectType, MemoryPool PoolStuff, typename... CreationArgs>
    std::expected<ObjectType*, MemoryAllocationError> Make(PoolStuff& pool, CreationArgs&&... args) {
        // If the pool exposes its block geometry, hold the object to it:
        // it must FIT (sizeof) and SIT straight (alignof). A 64-byte object
        // in a 16-byte-aligned block is fine; a 64-byte-ALIGNED object is
        // not -- placement new would land it on an unaligned address and
        // that is UB no sanitizer reliably forgives.
        if constexpr (requires { PoolStuff::BLOCK_SIZE; PoolStuff::BLOCK_ALIGN; }) {
            static_assert(sizeof(ObjectType) <= PoolStuff::BLOCK_SIZE, "block overflow");
            static_assert(alignof(ObjectType) <= PoolStuff::BLOCK_ALIGN, "block under-aligned");
        }

        auto raw_buffer = pool.raw_allocate();
        if(!raw_buffer) {
            return std::unexpected {raw_buffer.error()};
        }

        // Placement new the stuff at the given buffer
        // With the given arguments
        return ::new (*raw_buffer) ObjectType(std::forward<CreationArgs>(args)...);
    }

    template<MemoryPool PoolStuff, typename ObjectType>
    MemoryAllocationError Destroy(PoolStuff& pool, ObjectType* obj) {
        if (!obj) {
            return MemoryAllocationError::Ok;
        }
        std::destroy_at(obj);
        return pool.raw_deallocate(obj);
    }
}
