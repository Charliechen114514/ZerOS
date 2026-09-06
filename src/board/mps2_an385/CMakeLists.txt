# mps2_an385 板包(D14 一板一包):QEMU 通用 Cortex-M3 目标
# 无真机无厂商库;内存映射/外设型号取自 QEMU 11.0.3 info mtree/qtree
# 同一份 src/arch/arm_cortex_m3/ 直接服务两块板——port 薄的红利首次兑现

set(CMAKE_EXECUTABLE_SUFFIX .elf)

add_subdirectory(example)
