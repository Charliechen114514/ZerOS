/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   The first real-silicon demo: onboard LED (PC13, active low)
 *          blinking at RTOS task rhythm — not a HAL_Delay busy-loop,
 *          your own scheduler keeping the beat on actual silicon.
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/base/led.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "demo_setup.hpp"
#include "gpio.hpp"

namespace {

// PC13: onboard LED, active low (pull low to light up)
using LedPin =
    ZerOS::board::Gpio<ZerOS::board::GpioPort::C, 1u << 13, ZerOS::gpio::Direction::Output>;
using Led = ZerOS::gpio::LED<LedPin, ZerOS::gpio::Polarity::ActiveLow>;

void blinker(void*) {
    for (;;) {
        Led::toggle();
        ZerOS::ThisTask::sleep_for(ZerOS::Milliseconds{1000});
    }
}

} // namespace

int main() {
    ZerOS::demo::setup("blink");

    LedPin::init();

    ZerOS::arch::cortex_m3::launch(
        ZerOS::task::named("blink").prio(1).words(64).entry(blinker, nullptr));

    ZerOS::arch::cortex_m3::start_scheduler();
}
