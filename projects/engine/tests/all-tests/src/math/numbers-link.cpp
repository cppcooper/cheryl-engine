#include <math/string-numbers.h>

// A second independent include exercises header linkage when the aggregate is built.
double parsed_number_other_unit() {
    return CE::Text::Numbers::parse_floats("1.25");
}
