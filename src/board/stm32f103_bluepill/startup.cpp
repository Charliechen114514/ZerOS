// stm32f103_bluepill 启动:向量表 + .data 拷贝 + .bss 清零 + 故障兜底
#include <cstdint>

#include "stm32f1xx.h"
#include "uart.hpp"

extern "C" {
// 链接脚本符号(P1:只按整数地址比较,禁止指针比较)
extern std::uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

int main();
void SysTick_Handler();

void reset_handler() {
    auto src = reinterpret_cast<std::uintptr_t>(&_sidata);
    auto dst = reinterpret_cast<std::uintptr_t>(&_sdata);
    const auto dend = reinterpret_cast<std::uintptr_t>(&_edata);
    for (; dst < dend; dst += 4, src += 4) {
        *reinterpret_cast<std::uint32_t*>(dst) = *reinterpret_cast<std::uint32_t*>(src);
    }
    auto b = reinterpret_cast<std::uintptr_t>(&_sbss);
    const auto bend = reinterpret_cast<std::uintptr_t>(&_ebss);
    for (; b < bend; b += 4) {
        *reinterpret_cast<std::uint32_t*>(b) = 0;
    }

    SystemCoreClock = 72'000'000; // 板上主频;待真时钟树初始化取代
    main();
    for (;;) {
        __WFI();
    }
}

// 最小兜底:打印一行后停机;完整现场捕获属内核 HardFault 模块(roadmap 3.1)
void fault_handler() {
    ZerOS::board::print("\r\n[FAULT] halted\r\n");
    for (;;) {
        __WFI();
    }
}
} // extern "C"

using Isr = void (*)();

__attribute__((section(".isr_vector"), used))
const Isr vectors[] = {
    reinterpret_cast<Isr>(&_estack),             // 0  初始 SP
    &reset_handler,                              // 1  Reset
    &fault_handler,                              // 2  NMI
    &fault_handler,                              // 3  HardFault
    &fault_handler,                              // 4  MemManage
    &fault_handler,                              // 5  BusFault
    &fault_handler,                              // 6  UsageFault
    nullptr,                                     // 7-10 保留
    nullptr,
    nullptr,
    nullptr,
    nullptr,                                     // 11-14 (SVC/PendSV 归内核 port)
    nullptr,
    nullptr,
    nullptr,
    &SysTick_Handler,                            // 15 SysTick(应用定义)
};
