// stm32f103_bluepill 板级 UART1 实现

#include "uart.hpp"

#include "stm32f1xx.h"

#include <cstdint>

#include "ZerOS/log/global_channel.hpp"

namespace ZerOS::board {

void uart1_init() {
    // RCC 在 Renode 为假值(就绪位恒 1),此行主要服务真机
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
    // PA9: 复用推挽输出 2MHz;PA10: 浮空输入
    GPIOA->CRH = (GPIOA->CRH & ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9 | GPIO_CRH_MODE10 | GPIO_CRH_CNF10)) |
                 GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1 | GPIO_CRH_CNF10_0;
    USART1->BRR = 72'000'000 / 115'200;
    // RXNE 溢出(ORE)也会借 RXNEIE 拉起中断,handler 内自判 SR
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    // USART1 中断优先级必须压到 0x80(数值大于 BASEPRI 0x40):
    // 默认 0 是临界区罩不住的,调度器改队列改到一半会被它打断
    auto* const kNvicIp = reinterpret_cast<volatile std::uint8_t*>(0xE000E400);
    kNvicIp[kUsart1Irq] = 0x80;
    // ISER1 管 IRQ32-63,IRQ37 落 bit5
    auto* const kNvicIsr1 = reinterpret_cast<volatile std::uint32_t*>(0xE000E104);
    *kNvicIsr1 = (1u << (kUsart1Irq - 32));
}

void uart1_putc(char c) {
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
    USART1->DR = static_cast<std::uint8_t>(c);
}

char uart1_getc() {
    return static_cast<char>(USART1->DR & 0xFFu);
}

void print(const char* s) {
    while (*s != '\0') {
        uart1_putc(*s++);
    }
}

namespace {
// the channel body: everything log::print produces lands here, byte-polled
bool channel_body(const char* s) {
    while (*s != '\0') {
        uart1_putc(*s++);
    }
    return true;
}
} // namespace

void register_uart_channel() {
    ZerOS::log::RegisterGlobalChannel(&channel_body);
}

} // namespace ZerOS::board
