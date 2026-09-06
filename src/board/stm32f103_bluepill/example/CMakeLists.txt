# Example Registrytion — the demo sample plate.
#
# One scenario, one subdirectory. Each subdirectory registers itself with
# the one-liner:  zeros_add_demo(<name>)

# simulation seconds for every run-<name> target
set(ZEROS_SIM_SECONDS 5 CACHE STRING "仿真运行秒数")

# everything a demo links besides its own main.cpp: the arm bridge, the
# board startup and the common formatter bodies
set(ZEROS_DEMO_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/fault.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/launch.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/system.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/this_task.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/time_kernel_cm3.cpp
    ${CMAKE_SOURCE_DIR}/src/arch/arm_cortex_m3/switch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../uart.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../uart_drain.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../startup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../system_stm32f1xx.c
)

function(zeros_add_demo name)
    add_executable(zeros-demo-${name} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                                         ${ZEROS_DEMO_COMMON_SOURCES})
    target_link_libraries(zeros-demo-${name} PRIVATE zeros_common)

    target_include_directories(
        zeros-demo-${name}
        PRIVATE ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../..    # board root: uart.hpp, link.ld
        ${CMAKE_CURRENT_SOURCE_DIR}/..       # example/: demo_setup.hpp
        ${CMAKE_SOURCE_DIR}/third_party/STM32CubeF1/Drivers/CMSIS/Include
        ${CMAKE_SOURCE_DIR}/third_party/STM32CubeF1/Drivers/CMSIS/Device/ST/STM32F1xx/Include
    )
    target_compile_definitions(zeros-demo-${name} PRIVATE STM32F103xB)
    target_link_options(
        zeros-demo-${name}
        PRIVATE -nostdlib
        -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/../../link.ld
        -Wl,--gc-sections
        -Wl,--print-memory-usage
        -Wl,-Map,${CMAKE_BINARY_DIR}/demo-${name}.map
    )
    add_custom_command(
        TARGET zeros-demo-${name}
        POST_BUILD
        COMMAND arm-none-eabi-size $<TARGET_FILE:zeros-demo-${name}>
        COMMENT "size report: demo-${name}"
    )

    # build it AND watch it run: cmake --build build --target run-<name>
    add_custom_target(
        run-${name}
        COMMAND ${CMAKE_COMMAND}
        -DRENODE_ROOT=${CMAKE_SOURCE_DIR}
        -DRENODE_ELF=$<TARGET_FILE:zeros-demo-${name}>
        -DRENODE_RESC=src/board/stm32f103_bluepill/sim/renode/bluepill.resc
        -DRENODE_SECONDS=${ZEROS_SIM_SECONDS}
        -P ${CMAKE_CURRENT_LIST_DIR}/../../sim/renode/run-renode.cmake
        DEPENDS zeros-demo-${name}
        USES_TERMINAL
    )

    # flash to real silicon via ST-Link: cmake --build build --target flash-<name>
    # (board must be plugged in; LED demos are the intended users)
    add_custom_target(
        flash-${name}
        COMMAND openocd
        -f interface/stlink.cfg
        -f target/stm32f1x.cfg
        -c "program $<TARGET_FILE:zeros-demo-${name}> verify reset exit"
        DEPENDS zeros-demo-${name}
        USES_TERMINAL
        COMMENT "flashing demo-${name} to Blue Pill via ST-Link"
    )
endfunction()

add_subdirectory(heartbeat)
add_subdirectory(blink)
add_subdirectory(perf)
add_subdirectory(event_stream)
add_subdirectory(inversion)
add_subdirectory(bottom_half)
add_subdirectory(timers)
add_subdirectory(events_notify)
add_subdirectory(testaments)
add_subdirectory(round_robin)
