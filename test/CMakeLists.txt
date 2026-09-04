# Host-only unit tests. Cross builds (arm-none-eabi) never reach here:
# the root file guards this subdirectory behind ZEROS_BUILD_TESTS (OFF by default).
#
#   cmake -B build-host -DZEROS_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build-host
#   ctest --test-dir build-host --output-on-failure

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

# The kernel headers as an INTERFACE target so tests stay decoupled
# from the firmware build.
add_library(zeros_test_headers INTERFACE)
target_include_directories(zeros_test_headers INTERFACE ${CMAKE_SOURCE_DIR}/include)

function(zeros_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Catch2::Catch2WithMain zeros_test_headers)
    # Same hardening the smoke test ran with; ASan+UBSan are free bug finders on host.
    target_compile_options(${name} PRIVATE -Werror -fsanitize=address,undefined)
    target_link_options(${name} PRIVATE -fsanitize=address,undefined)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

zeros_add_test(test_bitmap)
zeros_add_test(test_bitmap_pool)
zeros_add_test(test_typeable)

# ---- negative compile test: the alignof guard in typeable.hpp ----
# Make must reject over-aligned objects AT COMPILE TIME. We prove the
# guard by compiling a snippet that tries exactly that and requiring the
# build to FAIL. The toolchain is pinned to C++23 so a failure can only
# come from the static_assert, never from a missing std::expected.
# (test_typeable.cpp passing doubles as the positive control.)
try_compile(zeros_make_overaligned_compiles
    ${CMAKE_CURRENT_BINARY_DIR}/neg-make-overaligned
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/neg_make_overaligned.cpp
    CMAKE_FLAGS
        -DCMAKE_CXX_STANDARD=23
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DINCLUDE_DIRECTORIES=${CMAKE_SOURCE_DIR}/include
)
if(zeros_make_overaligned_compiles)
    message(FATAL_ERROR
        "neg_make_overaligned.cpp compiled: Make's alignof guard is missing or broken")
endif()
