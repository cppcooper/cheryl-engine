#pragma once

#include <magic/compiler.h>

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace CE::LivingWorld {
    inline ::Magic::Catalog fixture_catalog() {
        ::Magic::Catalog catalog;
        catalog.operations.push_back({::Magic::Primitive::Heal, "heal"});
        return catalog;
    }
    inline ::Magic::PlanHandle fixture_plan(const ::Magic::Catalog& catalog) {
        std::ifstream input(LIVING_WORLD_S1, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot read retained S1 fixture");
        const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        auto compiled = ::Magic::compile(source, catalog);
        if (!compiled)
            throw std::runtime_error("Retained S1 fixture did not compile");
        return std::move(*compiled);
    }
}
