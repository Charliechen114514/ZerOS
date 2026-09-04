// stm32f103_bluepill 板级 UART1(PA9 TX)轮询输出
#pragma once

#include <cstdint>

#include "stm32f1xx.h"

namespace ZerOS::board {

inline void uart1_init() {
    // RCC 在 Renode 为假值(就绪位恒 1),此行主要服务真机
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
}

inline void uart1_putc(char c) {
    while ((USART1->SR & USART_SR_TXE) == 0) {
    }
    USART1->DR = static_cast<std::uint8_t>(c);
}

inline void print(const char* s) {
    while (*s != '\0') {
        uart1_putc(*s++);
    }
}

inline void printdec(std::uint32_t n) {
    char buf[11];
    char* p = buf + sizeof(buf);
    *--p = '\0';
    do {
        *--p = static_cast<char>('0' + n % 10);
        n /= 10;
    } while (n != 0);
    print(p);
}

} // namespace ZerOS::board
