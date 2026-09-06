/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Software timers: a one-shot fires 300ms in, a periodic beats
 *          every 250ms — neither spends a task while it waits.
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/kernel/clock/timer.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

namespace {

constinit ZerOS::clock::Timer g_oneshot{};
constinit ZerOS::clock::Timer g_periodic{};

std::uint32_t g_beats = 0;

void once(void*) { // tick context: short, no sleeping, no slow locks
    ZerOS::log::print("one-shot: fired, going cold\r\n");
}

void beat(void*) {
    ++g_beats;
    ZerOS::log::print("beat #{}\r\n", ZerOS::log::Dec{g_beats});
}

} // namespace

int main() {
    ZerOS::demo::setup("software timers");

    g_oneshot.oneshot(ZerOS::clock::Milliseconds{300}, once, nullptr);
    g_periodic.periodic(ZerOS::clock::Milliseconds{250}, beat, nullptr);

    ZerOS::arch::cortex_m3::start_scheduler();
}
