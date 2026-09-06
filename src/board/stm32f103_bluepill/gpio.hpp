// stm32f103_bluepill 板级 GPIO — 裸寄存器(BSRR/CRH/RCC 直写,零 HAL)
// 概念层见 include/ZerOS/base/gpio_base.hpp;满足 GpioOutputPin/InputPin
#pragma once

#include "ZerOS/base/gpio_base.hpp"

#include <bit>
#include <cstdint>

namespace ZerOS::board {

using gpio::Direction;
using gpio::Pull;

// GPIO 端口基址(stm32f1 系)
enum class GpioPort : std::uintptr_t {
    A = 0x40010800,
    B = 0x40010C00,
    C = 0x40011000,
};

template <GpioPort PORT, std::uint16_t MASK, Direction DIR, Pull PULL = Pull::None>
struct Gpio {
    // register accessor — int→pointer reinterpret_cast isn't a constant
    // expression (the rule that bit CMSIS, see docs/cpp-style.md);
    // a function sidesteps the static-member-literal requirement,
    // -Os folds the address to an immediate anyway
    static auto* reg(std::size_t offset) {
        return reinterpret_cast<volatile std::uint32_t*>(
            static_cast<std::uintptr_t>(PORT) + offset);
    }
    static constexpr std::uint16_t mask = MASK;
    static constexpr std::uint8_t pin = static_cast<std::uint8_t>(std::countr_zero(MASK));
    static constexpr Direction direction = DIR;

    // 寄存器偏移(stm32f1 GPIO,字对齐)
    static constexpr std::size_t OFF_CRL = 0x00;
    static constexpr std::size_t OFF_CRH = 0x04;
    static constexpr std::size_t OFF_IDR = 0x08;
    static constexpr std::size_t OFF_ODR = 0x0C;
    static constexpr std::size_t OFF_BSRR = 0x10;

    // RCC APB2ENR 的 IOPxEN 位
    static constexpr std::uint32_t rcc_enable_bit() {
        if constexpr (PORT == GpioPort::A) { return 1u << 2; }
        else if constexpr (PORT == GpioPort::B) { return 1u << 3; }
        else if constexpr (PORT == GpioPort::C) { return 1u << 4; }
        else { return 0; }
    }

    // 引脚配置值(stm32f1 的 4-bit 字段:CNF[1:0] MODE[1:0])
    // 输出推挽 2MHz: MODE=10 CNF=00 → 0b0010
    // 输入浮空:     MODE=00 CNF=01 → 0b0100
    // 输入上拉:     MODE=00 CNF=10 → 0b1000(ODR=1 选上拉)
    static constexpr std::uint32_t config_bits() {
        if constexpr (DIR == Direction::Output) { return 0b0010; }
        else if constexpr (PULL == Pull::Up) { return 0b1000; }
        else if constexpr (PULL == Pull::Down) { return 0b1000; }
        else { return 0b0100; }
    }

    static void init() {
        // 1) RCC 开 GPIO 时钟
        auto* const rcc_apb2enr = reinterpret_cast<volatile std::uint32_t*>(0x40021018);
        *rcc_apb2enr |= rcc_enable_bit();

        // 2) 配置 CRH 或 CRL 的对应 4-bit 字段
        //    pin 0-7 → CRL,pin 8-15 → CRH
        constexpr std::size_t cr_offset = (pin < 8) ? OFF_CRL : OFF_CRH;
        constexpr std::uint8_t field = pin % 8;
        constexpr std::uint32_t shift = field * 4;
        constexpr std::uint32_t clear = 0xFu << shift;

        *reg(cr_offset) = (*reg(cr_offset) & ~clear) | (config_bits() << shift);

        // 3) 上拉/下拉:写 ODR 选方向
        if constexpr (DIR == Direction::Input && PULL != Pull::None) {
            if constexpr (PULL == Pull::Up) {
                *reg(OFF_ODR) |= MASK;
            } else {
                *reg(OFF_ODR) &= ~MASK;
            }
        }
    }

    // BSRR:低 16 位 = set,高 16 位 = reset——原子单写,零读改写
    static void set() { *reg(OFF_BSRR) = MASK; }
    static void reset() { *reg(OFF_BSRR) = static_cast<std::uint32_t>(MASK) << 16; }

    // toggle:唯一需要读改写的操作(ODR 异或)
    static void toggle() { *reg(OFF_ODR) ^= MASK; }

    static bool level() { return (*reg(OFF_IDR) & MASK) != 0; }
};

} // namespace ZerOS::board
