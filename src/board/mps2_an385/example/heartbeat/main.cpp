/**
 * @file main.cpp
 * @author CharlieChen114514 (725610365@qq.com)
 * @brief   All right, your bootstrap example — mps2_an385/QEMU port
 *          (与 bluepill/heartbeat 同源零改:demo_setup 同名同形)
 * @version 0.1
 * @date 2026-09-06
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "ZerOS/arch/arm_cortex_m3/switch.hpp"
#include "ZerOS/kernel/sched/task.hpp"
#include "ZerOS/kernel/sched/this_task.hpp"
#include "ZerOS/log/print.hpp"
#include "demo_setup.hpp"
#include <cstdint>

struct HeartBeatPackage {
    uint32_t sleepy;
    const char* banner;
};

HeartBeatPackage banner{.sleepy = 500, .banner = "Hello! ZerOS\r\n"};
HeartBeatPackage task{.sleepy = 500, .banner = "A heartbeat example!\r\n"};

void heartbeat(void* args) {
    HeartBeatPackage* package = reinterpret_cast<HeartBeatPackage*>(args);
    for (;;) {
        ZerOS::log::print(package->banner);
        ZerOS::ThisTask::sleep_for({package->sleepy});
    }
}

int main() {
    ZerOS::demo::setup("heartbeat (mps2)");

    auto shape = ZerOS::task::named("HeartBeat").prio(1).words(64);

    // clone the shapes
    ZerOS::arch::cortex_m3::launch(shape.entry(heartbeat, &banner));
    ZerOS::arch::cortex_m3::launch(shape.entry(heartbeat, &task));

    ZerOS::arch::cortex_m3::start_scheduler();
}
