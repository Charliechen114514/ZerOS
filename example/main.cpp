#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/arch/arm_cortex_m3/time_kernel_cm3.hpp"
#include "ZerOS/base/borrowed_ptr.hpp"
#include "ZerOS/kernel/clock/clock.hpp"

namespace {

std::uint32_t tick_count = 0;

constexpr auto on_tick = +[](ZerOS::base::BorrowedPtr<ZerOS::clock::TimeWaiter> w) {
    ZerOS::board::print("tick ");
    ZerOS::board::printdec(++tick_count);
    ZerOS::board::print("\r\n");
    ZerOS::arch::cortex_m3::system_time.call_after_span(w, 1000);
};

constinit ZerOS::clock::TimeWaiter ticker{ZerOS::clock::Ticks{0}, on_tick};

} // namespace

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS time demo: SysTick -> TimeKernel @ Blue Pill\r\n");

    ZerOS::arch::cortex_m3::init_time(SystemCoreClock / 1000);
    ZerOS::arch::cortex_m3::system_time.call_after_span(&ticker, 1000);

    for (;;) {
        __WFI();
    }
}
