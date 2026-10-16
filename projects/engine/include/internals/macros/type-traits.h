#pragma once
#include <ctti/type_id.hpp>
#include <ctti/name.hpp>
#include <type_traits>

#define TYPEIDOF(val) ctti::type_id_of(val)
#define TYPEID(type) ctti::type_id_of<type>()
// Names are fully qualified std::string_view values; the expression form stays unevaluated.
#define TYPENAME(type) ctti::name_of<type>()
#define TYPENAMEOF(val) ctti::name_of<decltype(val)>()

#define ISCLASS(type) std::is_class_v<type>
#define ISDERIVED(base, type) std::is_base_of_v<base, type>
