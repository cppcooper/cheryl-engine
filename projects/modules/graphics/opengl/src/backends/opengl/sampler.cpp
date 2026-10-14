#include <backends/opengl/sampler.h>

#include "sampling-internal.h"

#include <utility>

namespace CE::Rendering::OpenGL {
    namespace {
        float resolve_anisotropy(const std::shared_ptr<ResourceLifetime>& lifetime, const CE::Rendering::Sampling::Options& options) {
            CE::Rendering::Sampling::validate(options);
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL sampler needs a resource lifetime");
            lifetime->require_current();
            CE::Rendering::OpenGL::Detail::require_no_gl_error("Cannot create a sampler with pending OpenGL errors");
            return options.anisotropy == CE::Rendering::Sampling::Anisotropy::Disabled ? 1 : Detail::maximum_anisotropy();
        }

        Handle create_sampler_handle(std::shared_ptr<ResourceLifetime> lifetime) {
            lifetime->require_current();
            GLuint id = 0;
            glGenSamplers(1, &id);
            try {
                CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL sampler creation failed");
                return {lifetime, ResourceKind::Sampler, id};
            } catch (...) {
                lifetime->discard_untracked(ResourceKind::Sampler, id);
                throw;
            }
        }

        GLint minification_filter(const CE::Rendering::Sampling::Options& options) {
            const bool nearest = options.minification == CE::Rendering::Sampling::Filter::Nearest;
            switch (options.mipmaps) {
                case CE::Rendering::Sampling::MipmapFilter::None:
                    return nearest ? GL_NEAREST : GL_LINEAR;
                case CE::Rendering::Sampling::MipmapFilter::Nearest:
                    return nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
                case CE::Rendering::Sampling::MipmapFilter::Linear:
                    return nearest ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unknown image mipmap mode");
        }

        GLint wrap_mode(const CE::Rendering::Sampling::Wrap wrap) {
            switch (wrap) {
                case CE::Rendering::Sampling::Wrap::ClampToEdge:
                    return GL_CLAMP_TO_EDGE;
                case CE::Rendering::Sampling::Wrap::Repeat:
                    return GL_REPEAT;
                case CE::Rendering::Sampling::Wrap::MirroredRepeat:
                    return GL_MIRRORED_REPEAT;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unknown image wrap mode");
        }
    }

    Sampler::Sampler(std::shared_ptr<ResourceLifetime> lifetime, const CE::Rendering::Sampling::Options& options)
    : CE::Rendering::Sampling::Sampler(options, resolve_anisotropy(lifetime, options)),
      handle_(create_sampler_handle(std::move(lifetime))),
      binding_unit_limit_(Detail::binding_unit_limit()) {
        const auto id = handle_.id();
        glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, minification_filter(options));
        glSamplerParameteri(
            id, GL_TEXTURE_MAG_FILTER, options.magnification == CE::Rendering::Sampling::Filter::Nearest ? GL_NEAREST : GL_LINEAR
        );
        glSamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap_mode(options.wrap_u));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap_mode(options.wrap_v));
        if (Detail::anisotropy_supported())
            glSamplerParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, effective_anisotropy());
        CE::Rendering::OpenGL::Detail::require_no_gl_error("OpenGL sampler configuration failed");
    }

    void Sampler::require_binding(const std::uint32_t unit) const {
        static_cast<void>(handle_.id());
        if (unit >= binding_unit_limit_)
            throw Exceptions::invalid_args(CE_HERE, "Sampler binding unit exceeds the current context's limit");
    }

    void Sampler::bind(const std::uint32_t unit) const {
        require_binding(unit);
        glBindSampler(unit, handle_.id());
    }
}
