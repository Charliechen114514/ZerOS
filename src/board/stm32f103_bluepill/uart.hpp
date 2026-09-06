// stm32f103_bluepill 板级 UART1(PA9 TX 轮询输出 / PA10 RX 中断接收)
#pragma once

#include <cstdint>

namespace ZerOS::board {

// USART1 全局中断编号(stm32f1 系),向量槽位 = 16 + 37 = 53
inline constexpr std::uint32_t kUsart1Irq = 37;

// 波特率/引脚/NVIC 就绪;RXNE 溢出(ORE)也会借 RXNEIE 拉起中断
void uart1_init();

// 轮询发一字节(锁等待 TXE)——任务/ISR/遗言上下文皆可
void uart1_putc(char c);

// 读 DR 顺带清 RXNE;调用方(handler)先判过 SR.RXNE
char uart1_getc();

void print(const char* s);

void register_uart_channel();

} // namespace ZerOS::board
