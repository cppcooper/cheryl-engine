#pragma once
#ifndef DRAW_INFO_H
#define DRAW_INFO_H
#include <assets/resources/shader.h>
#include <internals/exceptions.h>

#include <memory>

namespace CE {
    struct DrawInfo {
        std::shared_ptr<Assets::Shader> material;
        glm::mat4 model_matrix = glm::mat4(1.0f);
        Assets::ShaderPass camera;
        glm::vec3 position{0.0f};
        float scale = 1.f;
        float alpha = 1.f;
        void use_shader() const {
            if (!material)
                throw Exceptions::invalid_args(CE_HERE, "A draw requires a material");
            material->bind_pass(camera);
            material->bind_draw({model_matrix, alpha, scale, 0});
        }
    };
}
#endif
