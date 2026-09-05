#pragma once
#include <cstdint>

namespace ZerOS::sched {
/**
 * @brief State is the Task State
 *
 */
enum class TaskState : std::uint8_t { Ready, Running, Blocked };
using TaskPriority_t = std::uint8_t;
} // namespace ZerOS::sched