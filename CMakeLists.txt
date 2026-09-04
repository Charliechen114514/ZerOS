cmake_minimum_required(VERSION 3.20)
project(ZerOS C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

# Two disjoint configurations:
#   default          -> cross firmware (arm-none-eabi toolchain file)
#   ZEROS_BUILD_TESTS=ON -> host-only unit tests, no firmware targets
option(ZEROS_BUILD_TESTS "Build host unit tests" OFF)
if(ZEROS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
else()
    add_subdirectory(src/board/stm32f103_bluepill)
endif()
