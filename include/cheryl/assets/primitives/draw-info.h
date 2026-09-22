#pragma once
#ifndef DRAW_INFO_H
#define DRAW_INFO_H
#include <assets/abstracts/shader.h>

#include <memory>

namespace CE {
    struct DrawInfo {
        std::shared_ptr<Assets::Shader> material;
        glm::mat4 model_matrix = glm::mat4(1.0f);
        glm::vec3 position;
        float scale = 1.f;
        float alpha = 1.f;
        void use_shader() const {
            material->use();
            material->set_uniform_value("in_Alpha", alpha);
            material->set_uniform_value("in_Scale", scale);
            material->set_uniform_matrix("modelMatrix", model_matrix);
        }
    };
}
#endif
