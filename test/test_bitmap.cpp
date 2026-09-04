#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "ZerOS/base/bitmap.hpp"

using ZerOS::base::Bitmap;

TEST_CASE("bit level: set/clear/test round trip", "[bitmap]") {
    Bitmap<70> b; // 3 words: 32 + 32 + 6, exercises the tail word too
    for (std::size_t i = 0; i < 70; ++i) {
        b.set(i);
        CHECK(b.test(i));
    }
    for (std::size_t i = 0; i < 70; ++i) {
        b.clear(i);
        CHECK_FALSE(b.test(i));
    }
}

TEST_CASE("word level: empty/full and raw bulk access", "[bitmap]") {
    Bitmap<8> b;
    CHECK(b.word_empty(0));
    CHECK_FALSE(b.word_full(0));

    b.word(0) = 0xFFu; // raw write covering exactly the 8 real bits
    CHECK(b.word_full(0));
    CHECK(b.find_first_zero() == Bitmap<8>::npos);
    CHECK(b.find_first_set() == 0);
}

TEST_CASE("tail word: padding bits never fake a hit", "[bitmap][tail]") {
    Bitmap<5> b; // tail word carries 27 padding bits
    for (std::size_t i = 0; i < 5; ++i) {
        b.set(i);
    }

    CHECK(b.find_first_zero() == Bitmap<5>::npos);
    CHECK(b.first_zero_in_word(0) == Bitmap<5>::npos);
    CHECK(b.find_first_set() == 0);
}

TEST_CASE("find_first_zero crosses into the tail word", "[bitmap]") {
    Bitmap<70> b;
    for (std::size_t i = 0; i < 64; ++i) {
        b.set(i); // fill words 0 and 1 completely
    }

    CHECK(b.find_first_zero() == 64);
    CHECK(b.word_full(0));
    CHECK(b.word_full(1));
    CHECK_FALSE(b.word_full(2));

    b.set(64);
    CHECK(b.find_first_zero() == 65);
}

TEST_CASE("find_first_set skips empty words", "[bitmap]") {
    Bitmap<70> b;
    CHECK(b.find_first_set() == Bitmap<70>::npos);

    b.set(65); // deep inside the tail word
    CHECK(b.find_first_set() == 65);

    b.clear(65);
    b.set(33); // head of the second word
    CHECK(b.find_first_set() == 33);
}

TEST_CASE("first_zero_in_word pinpoints inside one word", "[bitmap]") {
    Bitmap<32> b;
    b.set(0);
    b.set(1);
    b.set(5);

    CHECK(b.first_zero_in_word(0) == 2);

    b.word(0) = ~0u; // full single-word bitmap
    CHECK(b.first_zero_in_word(0) == Bitmap<32>::npos);
}
