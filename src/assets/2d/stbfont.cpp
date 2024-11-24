#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <assets/2d/stbfont.h>
#include <math/anchor.h>
#include <ext/matrix_transform.hpp>
#include <core/resources/memory.h>

namespace CE::Assets {
    void STBFont::print(std::string text, FontDrawInfo* format) {
        print_msg = std::move(text);
        print_angle = format->angle;
        draw(*format);
    }

    void STBFont::draw(const DrawInfo& info) {
        const float scale = info.scale / 128;

        glBindVertexArray(vao.id);  // Bind the VAO for your font
        texture->bind();
        info.use_shader();  // Activate the shader

        glm::vec3 cursor_pos(info.position);
        auto model_matrix = glm::translate(glm::mat4(1.f), cursor_pos);
        model_matrix = glm::rotate(model_matrix, info.scale, glm::vec3(0.f, 0.f, 1.f));

        // Iterate over each character in the message
        for (char letter : print_msg) {
            info.material->set_uniform_matrix("modelMatrix", model_matrix);
            std::size_t index = letter-32;
            if (letter == '\n') {
                cursor_pos.y -= scale;
                //cursor_pos.y -= (info.scale / 2);
                model_matrix = glm::translate(glm::mat4(1.f), cursor_pos);
                model_matrix = glm::rotate(model_matrix, print_angle, glm::vec3(0.f, 0.f, 1.f));
            } else {
                using VAONumbers::vertices_per_quad;
                glDrawArrays(GL_QUADS, index * vertices_per_quad, vertices_per_quad);
                //model_matrix = glm::translate(model_matrix, glm::vec3(widths[index] * scale, 0.f, 0.f));
            }
        }
    }

    STBFontData STBFont::load_font(const char* font_path, int font_size) {
        constexpr std::size_t num_char = 96;
        stbtt_bakedchar baked_chars[num_char];
        using OPA = Mem::ObjectPoolAllocator<Texture>;
        using A_DA = std::allocator_traits<Mem::DefaultAllocator<unsigned char>>;
        using A_OPA = std::allocator_traits<OPA>;
        constexpr std::size_t pt20 = 1<<20;
        constexpr std::size_t km = 512 * 512;
        using sptr = std::shared_ptr<unsigned char>;

        // allocate font buffer
        sptr ttf_buffer(A_DA::allocate(pt20),[](unsigned char* p) {
            A_DA::deallocate(p, pt20);
        });
        fread(ttf_buffer.get(), 1, pt20, fopen(font_path, "rb"));

        // allocate bitmap
        sptr temp_bitmap(A_DA::allocate(km), [](unsigned char* p) {
            A_DA::deallocate(p,km);
        });

        stbtt_BakeFontBitmap(ttf_buffer.get(), 0, font_size, temp_bitmap.get(),
            512, 512, 32, 96, baked_chars);
        auto texture = std::shared_ptr<Texture>(A_OPA::allocate(1), [](Texture* p) {
            A_OPA::deallocate(p,1);
        });
        A_OPA::construct(texture.get(),
            temp_bitmap.get(),512,512,GL_TEXTURE0,
            true,false,GL_CLAMP_TO_EDGE,GL_ALPHA);

        constexpr std::size_t vertices_bytes = sizeof(Quad) * num_char;
        auto b = Mem::ExactMMgr::get().checkout_chunk(vertices_bytes, alignof(Vertex2D));
        std::shared_ptr<Vertex2D> vertices {
            static_cast<Vertex2D*>(b.head.get()),
            [b](Vertex2D* p) {
                Mem::ExactMMgr::get().return_chunk(b);
            }
        };
        std::size_t idx = 0;
        for(const auto &c : baked_chars) {
            Anchor::Center(vertices.get() + (idx++ * VAONumbers::floats_per_quad),
                        texture->width, texture->height, c.x1-c.x0,c.y1-c.y0, c.x0, c.y0);
        }
        return {vertices, num_char, texture};
    }
}
