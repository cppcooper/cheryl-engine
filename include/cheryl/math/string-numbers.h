#pragma once
#ifndef STRING_NUMBERS_H
#define STRING_NUMBERS_H
#include <cinttypes>
#include <limits>
#include <variant>
#include <stdexcept>
#include <charconv>
#include <string>

// Helper function to parse floating point numbers
double parse_floats(const std::string& str) {
    try {
        return std::stod(str);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Invalid floating point number.");
    } catch (const std::out_of_range& e) {
        throw std::runtime_error("Floating point number out of range.");
    }
}

// Using std::variant to return the smallest type possible
using NumberVariant = std::variant<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, double>;

template<bool is_unsigned>
NumberVariant parse_integers(const std::string& str) {
    if constexpr (is_unsigned) {
        uint64_t value;
        auto result = std::from_chars(str.data(), str.data() + str.size(), value);
        if (result.ec != std::errc()) throw std::runtime_error("Invalid integer format.");

        if (value <= std::numeric_limits<uint8_t>::max()) return static_cast<uint8_t>(value);
        if (value <= std::numeric_limits<uint16_t>::max()) return static_cast<uint16_t>(value);
        if (value <= std::numeric_limits<uint32_t>::max()) return static_cast<uint32_t>(value);
        return static_cast<uint64_t>(value);
    } else {
        int64_t value;
        auto result = std::from_chars(str.data(), str.data() + str.size(), value);
        if (result.ec != std::errc()) throw std::runtime_error("Invalid integer format.");

        if (value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max()) return static_cast<int8_t>(value);
        if (value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max()) return static_cast<int16_t>(value);
        if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max()) return static_cast<int32_t>(value);
        return static_cast<int64_t>(value);
    }
}

template<bool is_unsigned>
NumberVariant string_to_number(const std::string& str) {
    // Check if it's a floating-point number
    if (str.find('.') != std::string::npos || str.find('e') != std::string::npos || str.find('E') != std::string::npos) {
        return parse_floats(str);  // Always return double for floating point numbers
    } else {
        // Parse and return the appropriate integer type
        return parse_integers<is_unsigned>(str);
    }
}

#endif //STRING_NUMBERS_H
