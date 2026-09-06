# ZerOS: A Toy C++23 Embedded RTOS

![banner](documents/assets/banner.svg)

> Co-Apply for [Tutorial_AwesomeModernCPP](https://github.com/Awesome-Embedded-Learning-Studio/Tutorial_AwesomeModernCPP)

[![ci](https://github.com/Charliechen114514/ZerOS/actions/workflows/ci.yml/badge.svg)](https://github.com/Charliechen114514/ZerOS/actions/workflows/ci.yml)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![Cortex-M3](https://img.shields.io/badge/Cortex--M3-bare--metal-1f6feb?logo=arm&logoColor=white)](https://developer.arm.com/processors/cortex-m3)
![freestanding](https://img.shields.io/badge/freestanding-no_heap_%C2%B7_no_rtti_%C2%B7_no_exceptions-e91e63)
[![MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

**English** | [简体中文](README.md)

A component-oriented, C++23 bare-metal RTOS for Cortex-M3 — no heap, no RTTI,
no exceptions, no C ABI. One kernel, four homes: desktop unit tests,
Renode/QEMU simulation, and real silicon.

## Why it is written this way

- **The RTOS is a component, not the center.** Applications face only
  `<ZerOS/*>` headers and the BSP's public face; the kernel never learns a
  single board register. Swapping boards never touches the kernel.
- **All the C++23 you can keep on bare metal:** concepts-constrained
  interfaces, type-state task factories, `std::expected` on the application
  face — while the hot paths stay plain, static, and countable.
- **Four targets, one codebase:** host unit tests, Renode simulation,
  QEMU mps2-an385, and real silicon.

## What's inside

- 32-level preemptive priorities + same-level round-robin (bitmap + CLZ)
- Millisecond time services: `sleep_for`, software timers, timeouts;
  dynamic tick — while idle, SysTick parks as a one-shot to the next
  deadline, cutting tick interrupts by 99%+
- The synchronization family: semaphore, mutex (immediate priority
  inheritance), message queue (blocking send + ISR post), event group
  (broadcast), direct task notification (single-slot mailbox)
- Work queue (ISR bottom half), logging channels, stack canary +
  high-water mark, HardFault last words
- Sizing, measured on the demos: a few KB of flash for the kernel,
  worst demo ~6KB

## Quick start

Host tests — nothing but a C++ compiler and CMake needed:

```shell
cmake -B build-host -DZEROS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host -j && ctest --test-dir build-host --output-on-failure
```

Cross-build and watch it run in Renode (arm-none-eabi-gcc ≥ 14 + Renode
required; see `third_party/README.md` for the one-time submodule setup):

```shell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arch/arm-none-eabi.cmake
cmake --build build --target run-heartbeat -j   # build + Renode + UART, one shot
```

On QEMU instead:

```shell
cmake --build build --target run-mps2-heartbeat -j
```

On real silicon via ST-Link:

```shell
cmake --build build --target flash-heartbeat -j
```

## Demos

One scenario, one directory, one `zeros_add_demo(<name>)` line —
each carries its own `run-<name>` target.

| demo | shows |
|---|---|
| `heartbeat` | the bootstrap: two tasks, one beat |
| `blink` | GPIO/LED on real silicon |
| `event_stream` | timer-fed producer + real ISR injection |
| `inversion` | priority inheritance theater |
| `bottom_half` | ISR defers, a worker finishes |
| `timers` | one-shot + periodic software timers |
| `events_notify` | event-group AND + direct mailbox |
| `testaments` | canary/HardFault last words |
| `round_robin` | two spinners, zero yields, fair slices |
| `watermark` | stack high-water mark report |
| `tickless` | dynamic tick, witnessed (tick counts before/after) |
| `perf` | context-switch latency, measured on silicon |

## Layout

```
include/ZerOS/   public API, filed by subsystem (kernel/sched, kernel/clock, ...)
src/kernel/      the kernel — builds standalone on host, zero board includes
src/system/      private template implementation headers
src/arch/        architecture bridge (arm_cortex_m3, host)
src/log/         logging: formatting lives here, never inside the kernel
src/board/       one board, one package (bluepill, mps2_an385): glue + linker
                 script + sim assets + examples
third_party/     CMSIS headers via sparse submodule
documents/       engineering notes (performance measurement, in Chinese)
```

## License

MIT — see [LICENSE](LICENSE). The CMSIS device headers under `third_party/`
are ST's, Apache-2.0, fetched via git submodule.
