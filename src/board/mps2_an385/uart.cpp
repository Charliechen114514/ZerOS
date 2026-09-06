// mps2_an385 板级 UART0 实现(CMSDK APB UART)
// 型号与基址取自 QEMU 11.0.3 info qtree:dev cmsdk-apb-uart @ 0x40004000

#include "uart.hpp"

#include <cstdint>

#include "ZerOS/log/global_channel.hpp"

namespace ZerOS::board {

namespace {
// CMSDK APB UART 寄存器(字对齐,偏移即下标)
auto* const kData    = reinterpret_cast<volatile std::uint32_t*>(0x40004000);
auto* const kState   = reinterpret_cast<volatile std::uint32_t*>(0x40004004);
auto* const kCtrl    = reinterpret_cast<volatile std::uint32_t*>(0x40004008);
auto* const kBaudDiv = reinterpret_cast<volatile std::uint32_t*>(0x40004010);

constexpr std::uint32_t kStateTxBufferFull = (1u << 0);
constexpr std::uint32_t kCtrlTxEnable      = (1u << 0);
} // namespace

void uart0_init() {
    // 无引脚无时钟门控可配——QEMU 的外设上电即在;只喂分频与使能
    *kBaudDiv = 1;          // 全速:QEMU 模型对 chardev 直通,分频不构成节流
    *kCtrl = kCtrlTxEnable; // TXEN(接 RX 中断时再补 RXEN 与 NVIC)
}

void uart0_putc(char c) {
    while ((*kState & kStateTxBufferFull) != 0) {
    }
    *kData = static_cast<std::uint8_t>(c);
}

void print(const char* s) {
    while (*s != '\0') {
        uart0_putc(*s++);
    }
}

namespace {
// the channel body: everything log::print produces lands here, byte-polled
bool channel_body(const char* s) {
    while (*s != '\0') {
        uart0_putc(*s++);
    }
    return true;
}
} // namespace

void register_uart_channel() {
    ZerOS::log::RegisterGlobalChannel(&channel_body);
}

} // namespace ZerOS::board
