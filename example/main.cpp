#include <cstdint>
#include <expected>

#include "stm32f1xx.h"
#include "uart.hpp"

#include "ZerOS/kernel/mem/bitmap_allocate.hpp"
#include "ZerOS/kernel/mem/typeable.hpp"

namespace {

using ZerOS::memory::BitmapPool;
using ZerOS::memory::Destroy;
using ZerOS::memory::Make;
using ZerOS::memory::MemoryAllocationError;

struct Gadget {
    int value;
    static inline int alive = 0;

    explicit Gadget(int v) : value(v) {
        ++alive;
    }
    ~Gadget() {
        --alive;
    }
};

constinit BitmapPool<64, 8, true> pool{};

const char* err_name(MemoryAllocationError e) {
    switch (e) {
    case MemoryAllocationError::Ok: return "Ok";
    case MemoryAllocationError::OutOfMemory: return "OutOfMemory";
    case MemoryAllocationError::Poisoned: return "Poisoned";
    case MemoryAllocationError::NotOwned: return "NotOwned";
    default: return "?";
    }
}

void printhex(std::uintptr_t v) {
    static const char digits[] = "0123456789ABCDEF";
    char buf[11] = "0x";
    char* p = buf + 10;
    *p = '\0';
    for (int i = 0; i < 8; ++i) {
        *--p = digits[v & 0xFu];
        v >>= 4;
    }
    ZerOS::board::print(buf);
}

void report(const char* name, bool ok) {
    ZerOS::board::print(name);
    ZerOS::board::print(ok ? ": OK\r\n" : ": FAIL\r\n");
}

} // namespace

extern "C" void SysTick_Handler() {
    static std::uint32_t ticks = 0;
    if (++ticks % 1000 == 0) {
        ZerOS::board::print("tick ");
        ZerOS::board::printdec(ticks / 1000);
        ZerOS::board::print("\r\n");
    }
}

int main() {
    ZerOS::board::uart1_init();
    ZerOS::board::print("\r\nZerOS mem demo: BitmapPool + Make/Destroy @ Blue Pill\r\n");

    void* blocks[3] = {};
    for (int i = 0; i < 3; ++i) {
        auto r = pool.raw_allocate();
        if (r) {
            blocks[i] = *r;
        }
    }
    ZerOS::board::print("alloc3:");
    for (int i = 0; i < 3; ++i) {
        ZerOS::board::print(" ");
        printhex(reinterpret_cast<std::uintptr_t>(blocks[i]));
    }
    ZerOS::board::print("\r\n");
    report("distinct", blocks[0] != nullptr && blocks[1] != nullptr && blocks[2] != nullptr &&
                      blocks[0] != blocks[1] && blocks[1] != blocks[2]);

    pool.raw_deallocate(blocks[0]);
    auto reused = pool.raw_allocate();
    report("first-fit-reuse", reused.has_value() && *reused == blocks[0]);
    pool.raw_deallocate(blocks[1]);
    pool.raw_deallocate(blocks[2]);
    pool.raw_deallocate(*reused);

    auto g = Make<Gadget>(pool, 41);
    report("make", g.has_value() && (*g)->value == 41 && Gadget::alive == 1);
    report("destroy", Destroy(pool, *g) == MemoryAllocationError::Ok && Gadget::alive == 0);

    void* all[8] = {};
    for (int i = 0; i < 8; ++i) {
        auto r = pool.raw_allocate();
        if (r) {
            all[i] = *r;
        }
    }
    auto full = pool.raw_allocate();
    report("exhaustion", !full.has_value() && full.error() == MemoryAllocationError::OutOfMemory);
    ZerOS::board::print("exhaust err=");
    ZerOS::board::print(err_name(full ? MemoryAllocationError::Ok : full.error()));
    ZerOS::board::print("\r\n");
    for (int i = 0; i < 8; ++i) {
        pool.raw_deallocate(all[i]);
    }

    auto victim = pool.raw_allocate();
    pool.raw_deallocate(*victim);
    static_cast<unsigned char*>(*victim)[3] ^= 0xFF;
    auto poisoned = pool.raw_allocate();
    report("poison", !poisoned.has_value() && poisoned.error() == MemoryAllocationError::Poisoned);
    ZerOS::board::print("poison err=");
    ZerOS::board::print(err_name(poisoned.error()));
    ZerOS::board::print("\r\n");

    ZerOS::board::print("demo done\r\n");

    SysTick_Config(SystemCoreClock / 1000);
    for (;;) {
        __WFI();
    }
}
