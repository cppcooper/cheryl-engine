#include <core/resources/asset-management/texture-mgr.h>
#include <assets/2d/ffont.h>
#include <math/anchor.h>
#include <gtx/transform.hpp>
#include <fstream>
#include <mutex>

namespace CE::Assets {
    using namespace VAONumbers;
    constexpr uint16_t num_vertices = num_chars_ffont * vertices_per_quad;
    std::once_flag make_vertices_flag;
    std::array<Vertex2D, num_vertices> vertices{};
    struct NullDeleter {
        template<typename T>
        void operator()(T*) const noexcept {}
    };

    void FFont::print(std::string text, FontDrawInfo* format) {
        print_msg = std::move(text);
        print_fancy = reinterpret_cast<FFontFormat*>(format)->fancy;
        print_angle = format->angle;
        draw(*format);
    }

    void FFont::draw(const DrawInfo& info) {
        const float scale = info.scale / 128;
        glBindVertexArray(vao.id);
        texture->bind();
        info.use_shader();

        glm::vec3 cursor_pos(info.position);
        auto model_matrix = glm::translate(glm::mat4(1.f), cursor_pos);
        model_matrix = glm::rotate(model_matrix, info.scale, glm::vec3(0.f, 0.f, 1.f));

        for (auto letter : print_msg) {
            info.material->set_uniform_matrix("modelMatrix", model_matrix);
            GLint index = print_fancy ? letter-32+128 : letter-32;

            if (letter == '\n') {
                cursor_pos.y -= scale;
                //cursor_pos.y -= (info.scale / 2);
                model_matrix = glm::translate(glm::mat4(1.f), cursor_pos);
                model_matrix = glm::rotate(model_matrix, print_angle, glm::vec3(0.f, 0.f, 1.f));
            } else {
                glDrawArrays(GL_QUADS, index * vertices_per_quad, vertices_per_quad);
                model_matrix = glm::translate(model_matrix, glm::vec3(widths[index] * scale, 0.f, 0.f));
            }
        }
    }

    void make_vertices() {
        for (int idx = 0; idx < num_chars_ffont; ++idx) {
            uint16_t x0 = idx % 16;
            uint16_t y0 = idx / 16;
            math::Anchor::Center(vertices.data() + (idx * vertices_per_quad),
                                 16, 16, 1, 1, x0, y0);
        }
    }

    FFontData FFont::load_ffont(const std::filesystem::path& path) {
        std::fstream file(path);
        if (!file.is_open()) {
            // todo: throw
        }
        // todo: make sure we have the file we want
        std::array<short, num_chars_ffont> buffer{};
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size() * sizeof(short));
        file.close();

        std::array<float, num_chars_ffont> widths{};
        for (int idx = 0; idx < num_chars_ffont; ++idx) {
            widths[idx] = static_cast<float>(buffer[idx]);
        }

        std::call_once(make_vertices_flag, make_vertices);
        std::shared_ptr<Vertex2D> verts(vertices.data(), NullDeleter());
        static std::shared_ptr<Texture> texture = TextureMgr::get().get_asset("whitefont.png");
        return {widths, verts, num_vertices, texture};
    }
}
