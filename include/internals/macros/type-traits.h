#pragma once
#include <ctti/type_id.hpp>
#include <ctti/detailed_nameof.hpp>
#include <type_traits>

#define TYPEIDOF(val) ctti::type_id(val)
#define TYPEID(type) ctti::type_id<type>()
#define TYPENAME(type) ctti::detailed_nameof<type>().full_name()
#define TYPENAMEOF(val) ctti::detailed_nameof<decltype(val)>().full_name()

#define ISCLASS(type) std::is_class_v<type>
#define ISDERIVED(base,type) std::is_base_of_v<base, type>
