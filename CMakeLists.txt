cmake_minimum_required(VERSION 3.20)
project(ZerOS C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 只挂跨目标通用项;目标侧架构 flags 在 cmake/arch/arm-none-eabi.cmake(D14)
add_compile_options(-Wall -Wextra)

add_subdirectory(src/board/stm32f103_bluepill)
