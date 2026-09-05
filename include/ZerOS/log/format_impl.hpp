#pragma once

// The machinery beneath format_to. New printable types grow HERE: add the
// value wrapper below, then its emit_value overload in the same family —
// format.hpp itself never changes.

#include <cstdint>

namespace ZerOS::log {

struct Hex {
    std::uint32_t value; // 8 digits, zero padded, no 0x — write the prefix yourself
};

struct Dec {
    std::uint32_t value; // plain decimal, no padding
};

namespace format_impl {

// cursor walks the OUTPUT, limit is its hard wall (truncation, no overrun)
void emit(char*& cursor, char* limit, char c);
void emit_value(char*& cursor, char* limit, const char* text);
void emit_value(char*& cursor, char* limit, char c);
void emit_value(char*& cursor, char* limit, Hex h);
void emit_value(char*& cursor, char* limit, Dec d);

// fmt walks the FORMAT string; prints the plain text up to the next "{}"
// (or the end) and consumes it. Returns whether a placeholder was eaten —
// args without one stay silent.
[[nodiscard]] bool emit_plain(const char*& fmt, char*& cursor, char* limit);

} // namespace format_impl

} // namespace ZerOS::log
