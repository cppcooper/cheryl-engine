#include <math/string-numbers.h>
#include <gtest/gtest.h>
#include <limits>

double parsed_number_other_unit();

TEST(math_numbers, integer_widths) {
    EXPECT_EQ(std::get<int8_t>(parse_integers<false>("-128")), -128);
    EXPECT_EQ(std::get<int16_t>(parse_integers<false>("128")), 128);
    EXPECT_EQ(std::get<int32_t>(parse_integers<false>("32768")), 32768);
    EXPECT_EQ(std::get<int64_t>(parse_integers<false>("2147483648")), 2147483648LL);
    EXPECT_EQ(std::get<int64_t>(parse_integers<false>("-9223372036854775808")), std::numeric_limits<int64_t>::min());
    EXPECT_EQ(std::get<uint8_t>(parse_integers<true>("255")), 255);
    EXPECT_EQ(std::get<uint16_t>(parse_integers<true>("256")), 256);
    EXPECT_EQ(std::get<uint32_t>(parse_integers<true>("65536")), 65536u);
    EXPECT_EQ(std::get<uint64_t>(parse_integers<true>("18446744073709551615")), std::numeric_limits<uint64_t>::max());
}

TEST(math_numbers, complete_tokens) {
    for (const std::string token : {"", "12x", "12 ", " 12", "+12", "0x12", "-"}) {
        SCOPED_TRACE(token);
        EXPECT_THROW(parse_integers<false>(token), CE::Exceptions::invalid_args);
    }
    EXPECT_THROW(parse_integers<true>("-1"), CE::Exceptions::invalid_args);
    EXPECT_THROW(parse_integers<false>(std::string("12\0x", 4)), CE::Exceptions::invalid_args);
    for (const std::string token : {"", "1.5x", "1.5 ", "1e", "."}) {
        SCOPED_TRACE(token);
        EXPECT_THROW(parse_floats(token), CE::Exceptions::invalid_args);
    }
    EXPECT_THROW(parse_floats(std::string("1.5\0x", 5)), CE::Exceptions::invalid_args);
    EXPECT_EQ(parse_floats(" +1.5"), 1.5);
    EXPECT_EQ(std::get<double>(string_to_number<false>("1e2")), 100.0);
    EXPECT_EQ(std::get<int8_t>(string_to_number<false>("12")), 12);
    EXPECT_THROW(string_to_number<false>("12junk"), CE::Exceptions::invalid_args);
}

TEST(math_numbers, range_errors) {
    EXPECT_THROW(parse_integers<false>("9223372036854775808"), CE::Exceptions::bad_request);
    EXPECT_THROW(parse_integers<false>("-9223372036854775809"), CE::Exceptions::bad_request);
    EXPECT_THROW(parse_integers<true>("18446744073709551616"), CE::Exceptions::bad_request);
    EXPECT_THROW(parse_floats("1e9999"), CE::Exceptions::bad_request);
    EXPECT_THROW(parse_floats("1e-9999"), CE::Exceptions::bad_request);
}

TEST(math_numbers, header_linkage) {
    EXPECT_EQ(parse_floats("1.25"), parsed_number_other_unit());
}
