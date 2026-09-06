# Blue Pill 板包(D14 一板一包):固件目标 + 示例注册 + Renode 仿真目标
# 仿真资产在 sim/renode/(bluepill.repl/.resc);相对路径按启动 cwd 解析,须以仓库根为 cwd(P10)

set(CMAKE_EXECUTABLE_SUFFIX .elf)

set(CMSIS ${CMAKE_SOURCE_DIR}/third_party/STM32CubeF1/Drivers/CMSIS)

# ---- 示例注册中心:一个场景一个子目录,一行注册 ----
add_subdirectory(example)
# 仿真入口在 example 注册中心:每 demo 一个 run-<name> 目标
