#include <gtest/gtest.h>
#include <ctti/type_id.hpp>
#include <ctti/detailed_nameof.hpp>
#include <vector>
#include <iostream>

class Foo{};

class Bar{};

TEST(externlibs, ctti_type_id){
    int x;
    static_assert(ctti::type_id(x) == ctti::type_id<int>());
    std::vector<int> vx;
    std::vector<double> vy;
    ASSERT_TRUE(ctti::type_id<int>() == ctti::type_id(x));
    ASSERT_FALSE(ctti::type_id(vx) == ctti::type_id(vy));
}

TEST(externlibs, ctti_type_name) {
    std::vector<Foo> vf;
    ctti::name_t name_vi = ctti::detailed_nameof<std::vector<int>>();
    ctti::name_t name_vf = ctti::detailed_nameof<std::vector<Foo>>();
    ctti::name_t name_vf2 = ctti::detailed_nameof<decltype(vf)>();
    std::cout << name_vf.name() << std::endl;
    std::cout << name_vf.full_name() << std::endl;
    std::cout << name_vf.full_homogeneous_name() << std::endl;
    static_assert(ctti::detailed_nameof<Foo>() != ctti::detailed_nameof<Bar>());
    static_assert(ctti::detailed_nameof<std::vector<Foo>>() != ctti::detailed_nameof<std::vector<Bar>>());
    static_assert(ctti::detailed_nameof<std::vector<int>>() == ctti::detailed_nameof<std::vector<int>>());
    ASSERT_TRUE(name_vi.full_name() != name_vf.full_name());
    ASSERT_TRUE(name_vf.full_name() == name_vf2.full_name());
    ASSERT_TRUE(ctti::detailed_nameof<Foo>().full_name() == "Foo");
}
