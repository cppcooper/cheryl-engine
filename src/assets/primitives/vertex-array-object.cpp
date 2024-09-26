#include <assets/primitives/vertex-array-object.h>

template<typename T, uint64_t offset>
constexpr uint64_t get_length() {
    return sizeof(T) * offset;
}

template<typename T, uint64_t offset>
constexpr void* glBufferOffset() {
    return (void*)get_length<T, offset>();
}
namespace CE {
    using namespace VAONumbers;
    VAO::VAO(std::shared_ptr<Vertex2D> vertices, uint32_t num_vertices)
            : type(flat) {
        // each 2D vertex has 5 points of data: x,y,z,u,v
        constexpr GLsizei byte_stride = sizeof(Vertex2D);
        const GLsizei vertices_bytes = num_vertices * byte_stride;
        // prepare VAO
        glGenVertexArrays(1, &id_vao);
        glBindVertexArray(id_vao);
        // prepare VBO
        glGenBuffers(1, &id_vbo[1]);
        glBindBuffer(GL_ARRAY_BUFFER, id_vbo[1]);
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.get(), GL_STATIC_DRAW);
        // link VBO to position attrib
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        // link VBO to texture uv attrib
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
    }

    VAO::VAO(std::shared_ptr<Vertex3D> vertices, uint32_t num_vertices,
             std::shared_ptr<uint32_t> indices, uint32_t num_indices
    ) : type(mesh) {
        // each 3D vertex has 8 points of data: x,y,z,nx,ny,nz,u,v
        constexpr GLsizei byte_stride = sizeof(Vertex3D);
        const GLsizei vertices_bytes = num_vertices * byte_stride;
        const GLsizei size_vbo_indices = num_indices * sizeof(uint32_t);
        // prepare VAO
        glGenVertexArrays(1, &id_vao);
        glBindVertexArray(id_vao);
        // prepare VBOs
        glGenBuffers(2, id_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id_vbo[0]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_vbo_indices, indices.get(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, id_vbo[1]);
        glBufferData(GL_ARRAY_BUFFER, vertices_bytes, vertices.get(), GL_STATIC_DRAW);
        // link VBO to position attrib
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 0>());
        // link VBO to normal attrib
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 3>());
        // link VBO to texture uv attrib
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, byte_stride, glBufferOffset<float, 6>());
    }
}
