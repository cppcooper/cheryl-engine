#include <gtest/gtest.h>
#include <internals/macros/type-traits.h>
#include <ctti/type_id.hpp>
#include <ctti/name.hpp>
#include <vector>

class Foo {};

class Bar {};

namespace ctti_probe {
    class Item {};
}

TEST(externlibs, ctti_type_ids) {
    int number = 0;
    std::vector<int> integers;
    std::vector<double> decimals;

    // A value and its declared type agree; changing a container's element
    // type produces a different type ID.
    static_assert(ctti::type_id_of(number) == ctti::type_id_of<int>());
    EXPECT_EQ(ctti::type_id_of(number), ctti::type_id_of<int>());
    EXPECT_NE(ctti::type_id_of(integers), ctti::type_id_of(decimals));

    const int& reference = number;
    EXPECT_EQ(TYPEIDOF(reference), TYPEID(int));
    static_assert(TYPEID(const int&) != TYPEID(int));
}

TEST(externlibs, ctti_type_names) {
    std::vector<Foo> values;
    const auto integer_name = ctti::name_of<std::vector<int>>();
    const auto foo_name = ctti::name_of<std::vector<Foo>>();

    // Template arguments distinguish two vector types. A declared vector
    // and the type deduced from its instance still share the same name.
    static_assert(ctti::name_of<Foo>() != ctti::name_of<Bar>());
    static_assert(ctti::name_of<std::vector<Foo>>() != ctti::name_of<std::vector<Bar>>());
    EXPECT_NE(integer_name, foo_name);
    EXPECT_EQ(foo_name, ctti::name_of<decltype(values)>());
    EXPECT_EQ(ctti::name_of<Foo>(), "Foo");

    ctti_probe::Item item;
    EXPECT_EQ(TYPENAME(ctti_probe::Item), "ctti_probe::Item");
    EXPECT_EQ(TYPENAMEOF(item), "ctti_probe::Item");
    const auto& reference = item;
    static_assert(TYPENAMEOF(reference) == TYPENAME(const ctti_probe::Item&));
    static_assert(TYPENAMEOF(reference) != TYPENAME(ctti_probe::Item));
    int evaluations = 0;
    EXPECT_EQ(TYPENAMEOF(evaluations++), "int");
    EXPECT_EQ(evaluations, 0);
}
