#pragma once

// The chip's own flavor of the sync family: systems travel in the TYPE,
// so applications spell a plain name and never a template argument.

#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/kernel/sync/semaphore.hpp"

namespace ZerOS::sync {

using Semaphore = SemaphoreBase<arch::cortex_m3::CortexM3System>;

} // namespace ZerOS::sync
