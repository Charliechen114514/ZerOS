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
    template<MemoryPool PoolStuff, typename ObjectType, typename... CreationArgs>
    std::expected<ObjectType*, MemoryAllocationError> Make(PoolStuff& pool, CreationArgs&&... args) {
        // If, we have the pool block concept, then check it
        if constexpr (requires { PoolStuff::BLOCK_SIZE; }){
            static_assert(sizeof(ObjectType) <= PoolStuff::BLOCK_SIZE, "block overflow");
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
