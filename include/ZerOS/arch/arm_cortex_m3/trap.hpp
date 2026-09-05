#pragma once

/**
 * @brief Cortex M3 Trap
 *
 */
extern "C" [[noreturn]] inline void Trap() {
    while (1) {
        asm volatile("wfi");
    }
}
