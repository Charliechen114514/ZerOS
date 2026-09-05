// Bodies of the upper-layer formatter — ONE copy per binary, no matter how
// many translation units call format_to (the template shell calls these as
// plain functions instead of inlining the loops everywhere).

#include "ZerOS/log/format.hpp"

namespace ZerOS::log::format_impl {

void emit(char*& cursor, char* limit, char c) {
    if (cursor < limit) {
        *cursor++ = c;
    }
}

void emit_value(char*& cursor, char* limit, const char* text) {
    while (*text != '\0') {
        emit(cursor, limit, *text++);
    }
}

void emit_value(char*& cursor, char* limit, char c) {
    emit(cursor, limit, c);
}

void emit_value(char*& cursor, char* limit, Hex h) {
    constexpr char kDigits[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) { // fixed 8 digits, zero padded
        emit(cursor, limit, kDigits[(h.value >> shift) & 0xFu]);
    }
}

void emit_value(char*& cursor, char* limit, Dec d) {
    char tmp[10]; // 2^32 needs at most 10 digits
    int n = 0;
    do {
        tmp[n++] = static_cast<char>('0' + d.value % 10);
        d.value /= 10;
    } while (d.value != 0);
    while (n > 0) {
        emit(cursor, limit, tmp[--n]);
    }
}

bool emit_plain(const char*& fmt, char*& cursor, char* limit) {
    while (*fmt != '\0' && *fmt != '{') {
        emit(cursor, limit, *fmt++);
    }
    if (fmt[0] == '{' && fmt[1] == '}') { // a placeholder to consume
        fmt += 2;
        return true;
    }
    return false;
}

} // namespace ZerOS::log::format_impl
