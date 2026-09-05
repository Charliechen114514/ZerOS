add_library(
    zeros_common STATIC
    log/format_impl.cpp
)
target_include_directories(zeros_common PUBLIC ${CMAKE_SOURCE_DIR}/include)

if(NOT ZEROS_BUILD_TESTS) # boards only exist for the cross build
    add_subdirectory(board/stm32f103_bluepill)
endif()
