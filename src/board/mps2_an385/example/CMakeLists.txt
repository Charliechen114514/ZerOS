# 示例注册中心(mps2 版):一个场景一个子目录,一行注册
# 目标名带 mps2 前缀,与 bluepill 的同名 demo 并存不冲突:
# zeros-mps2-demo-<name>  ELF
# run-mps2-<name>         编 + 进 QEMU + 看串口,一发

set(ZEROS_SIM_SECONDS 5 CACHE STRING "仿真运行秒数")

# everything a demo links besides its own main.cpp: the arm bridge(与
# bluepill 共用同一套 arch 源)+ 板级启动与 UART
set(ZEROS_DEMO_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/fault.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/launch.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/system.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/this_task.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/time_kernel_cm3.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/switch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../uart.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../startup.cpp
)

function(zeros_add_demo name)
    add_executable(zeros-mps2-demo-${name} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${ZEROS_DEMO_COMMON_SOURCES})
    target_link_libraries(zeros-mps2-demo-${name} PRIVATE zeros_common)

    target_include_directories(
        zeros-mps2-demo-${name}
        PRIVATE ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_LIST_DIR}/../.. # board root: uart.hpp, link.ld
        ${CMAKE_CURRENT_LIST_DIR}/.. # example/: demo_setup.hpp
    )
    target_link_options(
        zeros-mps2-demo-${name}
        PRIVATE -nostdlib
        -Wl,-T,${CMAKE_CURRENT_LIST_DIR}/../../link.ld
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -Wl,-Map,${CMAKE_BINARY_DIR}/demo-${name}-mps2.map
    )
    add_custom_command(
        TARGET zeros-mps2-demo-${name}
        POST_BUILD
        COMMAND arm-none-eabi-size $<TARGET_FILE:zeros-mps2-demo-${name}>
        COMMENT "size report: demo-${name} (mps2)"
    )

    add_custom_target(
        run-mps2-${name}
        COMMAND ${CMAKE_COMMAND}
                -DQEMU_ROOT=${CMAKE_SOURCE_DIR}
                -DQEMU_ELF=$<TARGET_FILE:zeros-mps2-demo-${name}>
                -DQEMU_SECONDS=${ZEROS_SIM_SECONDS}
                -P ${CMAKE_CURRENT_LIST_DIR}/../../sim/qemu/run-qemu.cmake
        DEPENDS zeros-mps2-demo-${name}
        USES_TERMINAL
        COMMENT "running demo-${name} on QEMU mps2-an385"
    )
endfunction()

add_subdirectory(heartbeat)
