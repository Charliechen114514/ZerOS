/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   Events flow in, a task drains them: a timer drops one byte into
 *          a queue every 500ms (and a real UART interrupt may drop more —
 *          inject with `usart1 WriteChar 0x61`), the consumer echoes.
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/arch/arm_cortex_m3/sync.hpp"
#include "ZerOS/kernel/clock/timer.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

#include "stm32f1xx.h"

namespace {

constinit ZerOS::sync::Queue<char, 16> g_stream{};
constinit ZerOS::clock::Timer g_feeder{};

char g_next = 'a';

void drop_one(void*) { // tick context: one byte into the stream
    g_stream.post(g_next);
    ++g_next;
    if (g_next > 'f') {
        g_next = 'a';
    }
}

void consumer(void*) {
    for (;;) {
        auto got = g_stream.receive_one(); // wait forever — mailbox posture
        if (got.has_value()) {
            ZerOS::log::print("got '{}'\r\n", *got);
        }
    }
}

} // namespace

// a REAL uart interrupt may join the stream too — this strong handler
// overrides the board's weak drain
extern "C" void USART1_IRQHandler() {
    if ((USART1->SR & USART_SR_RXNE) != 0) {
        g_stream.post(ZerOS::board::uart1_getc());
    }
}

int main() {
    ZerOS::demo::setup("event stream");

    // 96 words: this task PRINTS per event — the line buffer rides its stack
    ZerOS::arch::cortex_m3::launch(ZerOS::task::named("consumer").prio(1).words(96).entry(consumer, nullptr));

    g_feeder.periodic(ZerOS::Milliseconds{500}, drop_one, nullptr);

    ZerOS::arch::cortex_m3::start_scheduler();
}
