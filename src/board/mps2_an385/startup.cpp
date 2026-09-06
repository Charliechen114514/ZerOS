// mps2_an385 启动:向量表 + .data 拷贝 + .bss 清零 + 故障兜底
// 与 bluepill/startup.cpp 同构,但无厂商库——裸寄存器,零外部依赖
#include <cstddef>
#include <cstdint>

#include "uart.hpp"

extern "C" {
// 链接脚本符号(P1:只按整数地址比较,禁止指针比较)
extern std::uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

int main();
void SysTick_Handler();   // SystemClock
void PendSV_Handler();    // PendSV
void SVC_Handler();       // Supervisor call
void HardFault_Handler(); // 现场捕获(arch fault.cpp)

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

    main();
    for (;;) {
        asm volatile("wfi");
    }
}

// 最小兜底:打印一行后停机;完整现场捕获属内核 HardFault 模块
void fault_handler() {
    ZerOS::board::print("\r\n[FAULT] halted\r\n");
    for (;;) {
        asm volatile("wfi");
    }
}

void* memset(void* dst, int value, std::size_t n) {
    auto* d = static_cast<unsigned char*>(dst);
    while (n-- != 0) {
        *d++ = static_cast<unsigned char>(value);
    }
    return dst;
}
} // extern "C"

using Isr = void (*)();

// mps2-an385 外设中断:cmsdk 定时器/UART 都在低号槽;未使能不会来,
// 万一误触发,兜底打印比 nullptr 的 lockup 好查(P4 精神)
__attribute__((section(".isr_vector"), used)) const Isr vectors[] = {
    reinterpret_cast<Isr>(&_estack), // 0  初始 SP
    &reset_handler,                  // 1  Reset
    &fault_handler,                  // 2  NMI
    &HardFault_Handler,              // 3  HardFault
    &HardFault_Handler,              // 4  MemManage(未使能则升级到 3)
    &HardFault_Handler,              // 5  BusFault(QEMU 对未映射访问直达此槽)
    &HardFault_Handler,              // 6  UsageFault
    nullptr,                         // 7-10 保留
    nullptr,
    nullptr,
    nullptr,
    &SVC_Handler,     // 11 SVC(首任务引导,内核 port)
    nullptr,          // 12 DebugMon
    nullptr,          // 13 保留
    &PendSV_Handler,  // 14 PendSV(内核 port)
    &SysTick_Handler, // 15 SysTick
    &fault_handler,   // 16-47 片上外设槽全兜底;接 UART RX 时精确填槽
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
    &fault_handler,
};
