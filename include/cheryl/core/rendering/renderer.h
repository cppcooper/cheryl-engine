#pragma once

#include <core/display/display-system-interface.h>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace CE::Assets {
    struct ResourceProvider;
}

namespace CE::RenderAPIs {
    namespace fs = std::filesystem;
    using program_id = std::uint64_t;

    struct iRenderer {
        std::unique_ptr<iDisplaySystem> display;
        virtual ~iRenderer() = default;

        virtual void initialize_libraries() = 0;
        virtual void initialize_rendering_context() = 0;
        virtual void deinitialize() = 0;
        virtual void clear() = 0;
        virtual void set_viewport(FramebufferSize size) = 0;
        virtual void swap_buffer() = 0;
        [[nodiscard]] virtual Assets::ResourceProvider& resources() = 0;
        virtual void draw() = 0;
        virtual program_id compile_shader(fs::path file) = 0;
    };
}

namespace CE {
    namespace R = RenderAPIs;
}
