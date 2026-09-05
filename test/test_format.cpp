#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <string>

#include "ZerOS/log/format.hpp"

using ZerOS::log::Dec;
using ZerOS::log::Hex;
using ZerOS::log::format_to;

namespace {

// run a format and give back what actually landed in the buffer
std::string run(const char* f) {
    char buf[128];
    return std::string(buf, format_to(buf, f));
}

template <typename... Args>
std::string run(const char* f, Args... args) {
    char buf[128];
    return std::string(buf, format_to(buf, f, args...));
}

} // namespace

TEST_CASE("plain text passes through untouched", "[format]") {
    CHECK(run("hello, bare metal") == "hello, bare metal");
    CHECK(run("") == "");
}

TEST_CASE("placeholders interleave with text in order", "[format]") {
    CHECK(run("task={} pc=0x{}", "H", Hex{0x08000F42u}) == "task=H pc=0x08000F42");
    CHECK(run("{}-{}-{}", "a", "b", "c") == "a-b-c");
    CHECK(run("n={} c={}", Dec{42u}, 'x') == "n=42 c=x");
}

TEST_CASE("hex is 8 digits zero padded, uppercase", "[format]") {
    CHECK(run("{}", Hex{0u}) == "00000000");
    CHECK(run("{}", Hex{0xDEADBEEFu}) == "DEADBEEF");
    CHECK(run("{}", Hex{0x0000000Fu}) == "0000000F");
}

TEST_CASE("decimal: plain, no padding, zero works", "[format]") {
    CHECK(run("{}", Dec{0u}) == "0");
    CHECK(run("{}", Dec{7u}) == "7");
    CHECK(run("{}", Dec{4294967295u}) == "4294967295");
}

TEST_CASE("buffer full: truncates, never overruns", "[format]") {
    char small[4];
    const auto n = format_to(small, "abcdefgh");
    CHECK(n == 4);
    CHECK(std::memcmp(small, "abcd", 4) == 0);
}

TEST_CASE("more args than placeholders: extras are ignored", "[format]") {
    CHECK(run("only {}", "used", "dropped") == "only used");
}

TEST_CASE("more placeholders than args: leftover {} stays visible", "[format]") {
    CHECK(run("got {} want {} - only one", "a") == "got a want {} - only one");
}
