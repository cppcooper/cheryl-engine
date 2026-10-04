#pragma once

#include <glad/gl.h>
#include <internals/exceptions.h>
#include <string>

namespace CE::RenderAPIs {
    // Call before creation to reject existing errors, then at each fallible upload
    // boundary. Never publish a resource after a reported native failure. In
    // particular, GL_OUT_OF_MEMORY does not guarantee queryable storage state.
    inline void require_no_gl_error(const char* operation) {
        const auto error = glGetError();
        if (error != GL_NO_ERROR)
            throw Exceptions::failed_operation(CE_HERE, std::string(operation) + " (GL error " + std::to_string(error) + ")");
    }
}
