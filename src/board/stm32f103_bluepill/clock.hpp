// stm32f103_bluepill 时钟树:HSI 8MHz 上电默认 → HSE 8MHz 晶振 → PLL ×9 = 72MHz
// 这是真机专属代码(Renode 的 RCC 模型是假值,P5);不叫它,一切
// 72MHz 假设(SysTick/UART/延时)都在 8MHz 下跑,全慢 9 倍
#pragma once

#include <cstdint>

namespace ZerOS::board {

inline void clock_init() {
    auto* const rcc_cr   = reinterpret_cast<volatile std::uint32_t*>(0x40021000);
    auto* const rcc_cfgr = reinterpret_cast<volatile std::uint32_t*>(0x40021004);
    auto* const flash_acr = reinterpret_cast<volatile std::uint32_t*>(0x40022000);

    // Simulation detector: Renode's RCC model has ready bits ALWAYS set
    // (P5: HSERDY=1 even when HSEON=0). On real silicon, HSERDY stays 0
    // until HSE is actually running — so seeing it high BEFORE we ask
    // for HSE means we're in a simulator with a fake clock tree. Skip
    // the whole thing; the sim already runs at whatever frequency it wants.
    if ((*rcc_cr & (1u << 17)) != 0) {
        return; // simulation — clock tree is fake, don't touch it
    }

    // 1) 开 HSE(外部 8MHz 晶振),等就绪
    *rcc_cr |= (1u << 16); // HSEON
    while ((*rcc_cr & (1u << 17)) == 0) { // HSERDY
    }

    // 2) Flash 等待周期:72MHz 需要 2 wait states
    *flash_acr = (*flash_acr & ~0x7u) | 0x2u;

    // 3) AHB 不分频(72MHz),APB1 ÷2(36MHz,最大),APB2 不分频(72MHz)
    //    CFGR: HPRE=0 (÷1), PPRE1=100 (÷2), PPRE2=0 (÷1)
    *rcc_cfgr = (*rcc_cfgr & ~(0xFu << 4) & ~(0x7u << 8) & ~(0x7u << 11))
              | (0x4u << 8); // PPRE1 = ÷2

    // 4) PLL:HSE × 9 = 72MHz
    //    CFGR: PLLSRC=1 (HSE), PLLXTPRE=0 (不÷2), PLLMUL=0111 (×9)
    *rcc_cfgr = (*rcc_cfgr & ~((1u << 16) | (1u << 17) | (0xFu << 18)))
              | (1u << 16)    // PLLSRC = HSE
              | (0x7u << 18); // PLLMUL = ×9

    // 5) 开 PLL,等就绪
    *rcc_cr |= (1u << 24); // PLLON
    while ((*rcc_cr & (1u << 25)) == 0) { // PLLRDY
    }

    // 6) 切系统时钟到 PLL 输出,等切换完成
    *rcc_cfgr = (*rcc_cfgr & ~0x3u) | 0x2u; // SW = PLL
    while ((*rcc_cfgr & (0x3u << 2)) != (0x2u << 2)) { // SWS = PLL
    }

    // 现在:SYSCLK=72MHz, AHB=72MHz, APB1=36MHz, APB2=72MHz
}

} // namespace ZerOS::board
