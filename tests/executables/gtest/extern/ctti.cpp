#include <gtest/gtest.h>
#include <ctti/type_id.hpp>
#include <ctti/detailed_nameof.hpp>
#include <vector>

class Foo {};

class Bar {};

TEST(externlibs, ctti_type_ids) {
    int number = 0;
    std::vector<int> integers;
    std::vector<double> decimals;

    // A value and its declared type agree; changing a container's element
    // type produces a different type ID.
    static_assert(ctti::type_id(number) == ctti::type_id<int>());
    EXPECT_EQ(ctti::type_id(number), ctti::type_id<int>());
    EXPECT_NE(ctti::type_id(integers), ctti::type_id(decimals));
}

TEST(externlibs, ctti_type_names) {
    std::vector<Foo> values;
    const auto integer_name = ctti::detailed_nameof<std::vector<int>>();
    const auto foo_name = ctti::detailed_nameof<std::vector<Foo>>();

    // Template arguments distinguish two vector types. A declared vector
    // and the type deduced from its instance still share the same name.
    static_assert(ctti::detailed_nameof<Foo>() != ctti::detailed_nameof<Bar>());
    static_assert(ctti::detailed_nameof<std::vector<Foo>>() != ctti::detailed_nameof<std::vector<Bar>>());
    EXPECT_NE(integer_name.full_name(), foo_name.full_name());
    EXPECT_EQ(foo_name.full_name(), ctti::detailed_nameof<decltype(values)>().full_name());
    EXPECT_EQ(ctti::detailed_nameof<Foo>().full_name(), "Foo");
}
