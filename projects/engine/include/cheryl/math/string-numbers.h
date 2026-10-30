#pragma once
#include <cinttypes>
#include <limits>
#include <variant>
#include <stdexcept>
#include <charconv>
#include <string>
#include <internals/exceptions.h>

namespace CE::Text::Numbers {

    /**
 * Parse a complete token. Floating syntax follows stod (including leading space,
 * a sign, and the current C locale); trailing data/space and embedded NULs fail.
 * Integer syntax is base-10 from_chars: no leading space or plus, with a minus
 * only for signed parsing. Invalid syntax throws invalid_args; a numeric value
 * outside the selected parser's range throws bad_request.
 * Floating parsing accepts stod's nonfinite values; it adds no finiteness policy.
 * @param str Complete floating token, borrowed only for this call.
 * @return Parsed double, including supported infinities/NaNs.
 * @throws CE::Exceptions::invalid_args For syntax or incomplete consumption.
 * @throws CE::Exceptions::bad_request For stod range failures.
 */
    inline double parse_floats(const std::string& str) {
        try {
            std::size_t consumed = 0;
            const auto value = std::stod(str, &consumed);
            if (consumed != str.size())
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid floating point number.");
            return value;
        } catch (const std::invalid_argument&) {
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid floating point number.");
        } catch (const std::out_of_range&) {
            throw ::CE::Exceptions::bad_request(CE_HERE, "Floating point number out of range.");
        }
    }

    /** Owned numeric result: smallest fitting requested integer type, or double.
     * No member borrows the parsed string. Inspect the active alternative for
     * signedness/width; this type does not preserve the original textual spelling.
     */
    using NumberVariant = std::variant<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, double>;

    /** Parses one complete base-10 integer token into its smallest fitting alternative.
     * @tparam is_unsigned True selects uint64 parsing; false selects int64 parsing.
     * @param str Entire token: no whitespace/plus; a minus is allowed only for signed parsing.
     * @return Owned numeric alternative; nonnegative signed input still produces a signed type.
     * @throws CE::Exceptions::invalid_args For empty, partial, NUL-containing or malformed input.
     * @throws CE::Exceptions::bad_request For values outside the selected 64-bit parser range.
     */
    template <bool is_unsigned> NumberVariant parse_integers(const std::string& str) {
        if constexpr (is_unsigned) {
            uint64_t value;
            auto result = std::from_chars(str.data(), str.data() + str.size(), value);
            if (result.ptr != str.data() + str.size() || result.ec == std::errc::invalid_argument)
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid integer format.");
            if (result.ec == std::errc::result_out_of_range)
                throw ::CE::Exceptions::bad_request(CE_HERE, "Integer out of range.");
            if (result.ec != std::errc())
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid integer format.");

            if (value <= std::numeric_limits<uint8_t>::max())
                return static_cast<uint8_t>(value);
            if (value <= std::numeric_limits<uint16_t>::max())
                return static_cast<uint16_t>(value);
            if (value <= std::numeric_limits<uint32_t>::max())
                return static_cast<uint32_t>(value);
            return static_cast<uint64_t>(value);
        } else {
            int64_t value;
            auto result = std::from_chars(str.data(), str.data() + str.size(), value);
            if (result.ptr != str.data() + str.size() || result.ec == std::errc::invalid_argument)
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid integer format.");
            if (result.ec == std::errc::result_out_of_range)
                throw ::CE::Exceptions::bad_request(CE_HERE, "Integer out of range.");
            if (result.ec != std::errc())
                throw ::CE::Exceptions::invalid_args(CE_HERE, "Invalid integer format.");

            if (value >= std::numeric_limits<int8_t>::min() && value <= std::numeric_limits<int8_t>::max())
                return static_cast<int8_t>(value);
            if (value >= std::numeric_limits<int16_t>::min() && value <= std::numeric_limits<int16_t>::max())
                return static_cast<int16_t>(value);
            if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max())
                return static_cast<int32_t>(value);
            return static_cast<int64_t>(value);
        }
    }

    /** Selects the legacy integer or floating parser from token spelling.
     * @tparam is_unsigned Signedness of integer parsing; does not restrict floating results.
     * @param str Complete token. A dot, e or E selects parse_floats; other tokens use parse_integers.
     * @return Owned NumberVariant; floating parsing always returns double.
     * @throws CE::Exceptions::invalid_args For malformed or partially consumed input.
     * @throws CE::Exceptions::bad_request For numeric range failures.
     * No base autodetection is performed; special floating tokens without a dot/e/E
     * are not selected automatically. Floating finiteness is not enforced.
     * @see [Utility contracts](cheryl-guide:runtime/utility-contracts)
     */
    template <bool is_unsigned> NumberVariant string_to_number(const std::string& str) {
        // Preserve legacy selection: '.', 'e', or 'E' chooses the floating parser;
        // otherwise use the requested signed/unsigned integer parser. This does not
        // auto-detect bases or special floating values without those markers.
        if (str.find('.') != std::string::npos || str.find('e') != std::string::npos || str.find('E') != std::string::npos) {
            return parse_floats(str); // Always return double for floating point numbers
        } else {
            // Parse and return the appropriate integer type
            return parse_integers<is_unsigned>(str);
        }
    }

}
