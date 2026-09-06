// The Cortex-M3 answer to the ThisTask face: it only knows the handover
// point (system::os()), never the singletons behind it.

#include "system/this_task_impl.hpp"

#include "ZerOS/arch/arm_cortex_m3/system.hpp"

namespace {

using ZerOS::system::os;

} // namespace

namespace ZerOS::detail {

struct SystemTaskBackend {
    static void sleep_for(Milliseconds delay) noexcept {
        auto& sys = os();
        sys.sleep_for(sys.current_task(), delay.count);
    }

    static void yield() noexcept { os().yield(); }

    static void block() noexcept {
        auto& sys = os();
        sys.block(sys.current_task());
    }

    static ZerOS::clock::Ticks now() noexcept { return os().now(); }

    static std::expected<std::uint32_t, ZerOS::sync::SyncError>
    wait_notify(Milliseconds timeout) noexcept {
        return os().wait_notify(timeout.count);
    }
};

} // namespace ZerOS::detail

namespace ZerOS {
// stamp the face for THIS program; business .o files link plain calls
template struct BasicThisTask<detail::SystemTaskBackend>;
} // namespace ZerOS
