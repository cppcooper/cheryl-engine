#pragma once
#include "bounded-stream.h"
#include <backward.hpp>
#include <ostream>
#include <string>

namespace CE::TraceDetail {
    /** May allocate while resolving symbols/copying text; callers choose fallback. */
    template <std::size_t Capacity>
    std::string capture(void* address, std::size_t depth, std::size_t skip) {
        static thread_local Diagnostics::BoundedStreamBuffer<Capacity> buffer;
        static thread_local std::ostream stream(&buffer);
        buffer.reset();
        stream.clear();
        backward::StackTrace stack;
        if (address)
            stack.load_from(address, depth);
        else
            stack.load_here(depth);
        backward::TraceResolver resolver;
        resolver.load_stacktrace(stack);
        for (std::size_t index = skip; index < stack.size(); ++index) {
            const auto trace = resolver.resolve(stack[index]);
            stream << '#' << index - skip << ' ' << trace.object_function << '[' << trace.addr << "] in "
                   << trace.source.filename << ':' << trace.source.line << ':' << trace.source.col << '\n';
        }
        return std::string(buffer.view());
    }
}
