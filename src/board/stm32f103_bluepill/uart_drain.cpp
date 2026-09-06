// Default RX consumer, its OWN translation unit: when an image has nobody
// listening, bytes rain into this drain. A demo that wants them defines
// its own strong handler and the linker drops this weak one.

#include "stm32f1xx.h"
#include "uart.hpp"

extern "C" void USART1_IRQHandler() __attribute__((weak));
extern "C" void USART1_IRQHandler() {
    if ((USART1->SR & USART_SR_RXNE) != 0) {
        (void)USART1->DR; // read DR, drop the byte, clear the flag
    }
}
