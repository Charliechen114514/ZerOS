# Blue Pill 板包(D14 一板一包):固件目标 + Renode 仿真目标
# 仿真资产在 sim/renode/(bluepill.repl/.resc);相对路径按启动 cwd 解析,须以仓库根为 cwd(P10)

set(CMAKE_EXECUTABLE_SUFFIX .elf)

set(CMSIS ${CMAKE_SOURCE_DIR}/third_party/STM32CubeF1/Drivers/CMSIS)

# ---- 固件目标 ----
add_executable(
    zeros-bluepill
    ${CMAKE_SOURCE_DIR}/example/main.cpp
    startup.cpp
    system_stm32f1xx.c
)
target_include_directories(
    zeros-bluepill
    PRIVATE ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMSIS}/Include
            ${CMSIS}/Core/Include
            ${CMSIS}/Device/ST/STM32F1xx/Include
)
target_compile_definitions(zeros-bluepill PRIVATE STM32F103xB)
target_link_options(
    zeros-bluepill
    PRIVATE -nostdlib
            -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/link.ld
            -Wl,--gc-sections
            -Wl,--print-memory-usage
            -Wl,-Map,${CMAKE_BINARY_DIR}/zeros-bluepill.map
)
add_custom_command(
    TARGET zeros-bluepill
    POST_BUILD
    COMMAND arm-none-eabi-size $<TARGET_FILE:zeros-bluepill>
    COMMENT "size report (D12 预算:核心内核 flash<=6KB RAM<=1KB)"
)

# ---- 仿真目标:cmake --build build --target renode-bluepill ----
set(ZEROS_SIM_ELF "" CACHE STRING "覆盖仿真 ELF(空=本板默认产物)")
set(ZEROS_SIM_SECONDS 5 CACHE STRING "仿真运行秒数")

if(ZEROS_SIM_ELF)
    set(_elf ${ZEROS_SIM_ELF})
else()
    set(_elf $<TARGET_FILE:zeros-bluepill>)
endif()

add_custom_target(
    renode-bluepill
    COMMAND ${CMAKE_COMMAND}
            -DRENODE_ROOT=${CMAKE_SOURCE_DIR}
            -DRENODE_ELF=${_elf}
            -DRENODE_RESC=src/board/stm32f103_bluepill/sim/renode/bluepill.resc
            -DRENODE_SECONDS=${ZEROS_SIM_SECONDS}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/sim/renode/run-renode.cmake
    DEPENDS zeros-bluepill
    USES_TERMINAL
)
