#pragma once

#include <cstdint>

namespace ZerOS::sync {

// the shared verdict of the synchronization family (and of anything that
// sleeps with a deadline — hence its own dependency-free home)
enum class SyncError : std::uint8_t { Ok, TimedOut };

} // namespace ZerOS::sync
