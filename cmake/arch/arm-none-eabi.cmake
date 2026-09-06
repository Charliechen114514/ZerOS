# arm-none-eabi 交叉工具链文件
# 用法:cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arch/arm-none-eabi.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# 裸机目标没有 main(),编译器自检只编静态库
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# 目标侧编译基线(D14:挂工具链文件而非根 CMakeLists,host 目标不受污染)
# Debug 构建用 -Og(调试可读)替代 -Os(体积最小):
#   cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug ...
# -Os 会重排基本块/内联/消除变量,GDB 断点打不中(「飞了」的根因)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ZEROS_OPT_LEVEL -Og)
else()
    set(ZEROS_OPT_LEVEL -Os)
endif()

add_compile_options(
    -mcpu=cortex-m3
    -mthumb
    -ffreestanding
    ${ZEROS_OPT_LEVEL}
    -g3
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
)
