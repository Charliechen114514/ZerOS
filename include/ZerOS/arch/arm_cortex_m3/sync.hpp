#pragma once

// The chip's own flavor of the sync family: systems travel in the TYPE,
// so applications spell a plain name and never a template argument.

#include "ZerOS/arch/arm_cortex_m3/system.hpp"
#include "ZerOS/kernel/sync/event_group.hpp"
#include "ZerOS/kernel/sync/msg_queue.hpp"
#include "ZerOS/kernel/sync/mutex.hpp"
#include "ZerOS/kernel/sync/semaphore.hpp"
#include "ZerOS/kernel/sync/user_worker.hpp"

namespace ZerOS::sync {

using Semaphore = SemaphoreBase<arch::cortex_m3::CortexM3System>;
using Mutex = MutexBase<arch::cortex_m3::CortexM3System>;
template <typename Message, std::size_t Capacity>
using Queue = QueueBase<arch::cortex_m3::CortexM3System, Message, Capacity>;
using Worker = WorkBase<arch::cortex_m3::CortexM3System>;
using Event = EventBase<arch::cortex_m3::CortexM3System>;

} // namespace ZerOS::sync
