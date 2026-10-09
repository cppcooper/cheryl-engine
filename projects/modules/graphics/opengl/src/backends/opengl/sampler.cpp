#include <backends/opengl/sampler.h>

#include "sampling-internal.h"

#include <utility>

namespace CE::Assets {
    namespace {
        float resolve_anisotropy(const std::shared_ptr<RenderAPIs::OpenGLResourceLifetime>& lifetime, const SamplerOptions& options) {
            validate_sampler_options(options);
            if (!lifetime)
                throw Exceptions::invalid_args(CE_HERE, "OpenGL sampler needs a resource lifetime");
            lifetime->require_current();
            RenderAPIs::require_no_gl_error("Cannot create a sampler with pending OpenGL errors");
            return options.anisotropy == ImageAnisotropy::Disabled ? 1 : RenderAPIs::SamplingDetail::maximum_anisotropy();
        }

        RenderAPIs::OpenGLHandle create_sampler_handle(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime) {
            lifetime->require_current();
            GLuint id = 0;
            glGenSamplers(1, &id);
            try {
                RenderAPIs::require_no_gl_error("OpenGL sampler creation failed");
                return {lifetime, RenderAPIs::GLResourceKind::Sampler, id};
            } catch (...) {
                lifetime->discard_untracked(RenderAPIs::GLResourceKind::Sampler, id);
                throw;
            }
        }

        GLint minification_filter(const SamplerOptions& options) {
            const bool nearest = options.minification == ImageFilter::Nearest;
            switch (options.mipmaps) {
                case MipmapFilter::None:
                    return nearest ? GL_NEAREST : GL_LINEAR;
                case MipmapFilter::Nearest:
                    return nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
                case MipmapFilter::Linear:
                    return nearest ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unknown image mipmap mode");
        }

        GLint wrap_mode(const ImageWrap wrap) {
            switch (wrap) {
                case ImageWrap::ClampToEdge:
                    return GL_CLAMP_TO_EDGE;
                case ImageWrap::Repeat:
                    return GL_REPEAT;
                case ImageWrap::MirroredRepeat:
                    return GL_MIRRORED_REPEAT;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unknown image wrap mode");
        }
    }

    OpenGLSampler::OpenGLSampler(std::shared_ptr<RenderAPIs::OpenGLResourceLifetime> lifetime, const SamplerOptions& options)
    : Sampler(options, resolve_anisotropy(lifetime, options)),
      handle_(create_sampler_handle(std::move(lifetime))),
      binding_unit_limit_(RenderAPIs::SamplingDetail::binding_unit_limit()) {
        const auto id = handle_.id();
        glSamplerParameteri(id, GL_TEXTURE_MIN_FILTER, minification_filter(options));
        glSamplerParameteri(id, GL_TEXTURE_MAG_FILTER, options.magnification == ImageFilter::Nearest ? GL_NEAREST : GL_LINEAR);
        glSamplerParameteri(id, GL_TEXTURE_WRAP_S, wrap_mode(options.wrap_u));
        glSamplerParameteri(id, GL_TEXTURE_WRAP_T, wrap_mode(options.wrap_v));
        if (RenderAPIs::SamplingDetail::anisotropy_supported())
            glSamplerParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, effective_anisotropy());
        RenderAPIs::require_no_gl_error("OpenGL sampler configuration failed");
    }

    void OpenGLSampler::require_binding(const std::uint32_t unit) const {
        static_cast<void>(handle_.id());
        if (unit >= binding_unit_limit_)
            throw Exceptions::invalid_args(CE_HERE, "Sampler binding unit exceeds the current context's limit");
    }

    void OpenGLSampler::bind(const std::uint32_t unit) const {
        require_binding(unit);
        glBindSampler(unit, handle_.id());
    }
}
