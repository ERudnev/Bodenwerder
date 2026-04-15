#include <Raidenmamare/primitives/base.q1.h>

#include <GLFW/glfw3.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rmmr::primitive {
    struct Base_private : Base::Operations {
        using Passport = Base::Materializer::Passport;
        using Runtime = Base::RuntimeStorage;

        static auto create_runtime(GLFWwindow* window, const Quantum& quantum) -> Runtime {
            if (!window) {
                throw std::runtime_error("primitive::Base::Materializer::materialize: device is not open");
            }

            if (quantum.positions.empty()) {
                throw std::runtime_error("primitive::Base::Materializer::materialize: positions are empty");
            }

            const auto pos_id = GeometrySemantics::id_of("position");
            const auto normal_id = GeometrySemantics::id_of("normal");

            const bool position_only = quantum.passport.layout.size() == std::size_t{1} && quantum.passport.layout[0] == pos_id;
            const bool position_normal = quantum.passport.layout.size() == std::size_t{2} && quantum.passport.layout[0] == pos_id
                && quantum.passport.layout[1] == normal_id;

            if (!position_only && !position_normal) {
                throw std::runtime_error(
                    "primitive::Base::Materializer::materialize: unsupported vertex layout (expect position only, or position+normal)");
            }

            if (position_only) {
                if (!quantum.normals.empty()) {
                    throw std::runtime_error("primitive::Base::Materializer::materialize: normals must be empty for position-only layout");
                }
            } else {
                if (quantum.normals.size() != quantum.positions.size()) {
                    throw std::runtime_error("primitive::Base::Materializer::materialize: normals count must match positions");
                }
            }

            glfwMakeContextCurrent(window);

            Runtime runtime{};
            glGenVertexArrays(1, &runtime.vao);
            glGenBuffers(1, &runtime.vbo);

            if (!runtime.vao || !runtime.vbo) {
                if (runtime.vao) glDeleteVertexArrays(1, &runtime.vao);
                if (runtime.vbo) glDeleteBuffers(1, &runtime.vbo);
                throw std::runtime_error("primitive::Base::Materializer::materialize: failed to allocate VAO/VBO");
            }

            const std::size_t vertex_count = quantum.positions.size();
            std::vector<float> interleaved;

            if (position_only) {
                interleaved.reserve(vertex_count * 3);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = quantum.positions[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                }

                constexpr GLsizei stride = static_cast<GLsizei>(3 * sizeof(float));

                glBindVertexArray(runtime.vao);
                glBindBuffer(GL_ARRAY_BUFFER, runtime.vbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                    interleaved.data(),
                    GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0)));
                glEnableVertexAttribArray(0);
            } else {
                interleaved.reserve(vertex_count * 6);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = quantum.positions[i];
                    const auto& n = quantum.normals[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(n.x);
                    interleaved.push_back(n.y);
                    interleaved.push_back(n.z);
                }

                constexpr GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));

                glBindVertexArray(runtime.vao);
                glBindBuffer(GL_ARRAY_BUFFER, runtime.vbo);
                glBufferData(
                    GL_ARRAY_BUFFER,
                    static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                    interleaved.data(),
                    GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0)));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(
                    1,
                    3,
                    GL_FLOAT,
                    GL_FALSE,
                    stride,
                    reinterpret_cast<void*>(static_cast<std::uintptr_t>(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
            }

            runtime.vertex_count = static_cast<integer>(vertex_count);
            return runtime;
        }

        static void release_runtime(GLFWwindow* window, Runtime& runtime) {
            if (!runtime.vao && !runtime.vbo) return;

            if (window) {
                glfwMakeContextCurrent(window);
            }

            if (runtime.vao) {
                glDeleteVertexArrays(1, &runtime.vao);
                runtime.vao = 0;
            }
            if (runtime.vbo) {
                glDeleteBuffers(1, &runtime.vbo);
                runtime.vbo = 0;
            }
            runtime.vertex_count = 0;
        }
    };

    void Base::Materializer::materialize(resources::Manager manager, Reading world, Id id) const {
        if (manager->materialized<Base>(id)) {
            throw std::runtime_error("primitive::Base::Materializer::materialize: resource is already materialized");
        }

        const auto& quantum = ops::particle::get<Base>(world, id);
        GLFWwindow* window = Device::Operations::provide(world, quantum.device);
        manager->layer<Base>().materialize(id, Base_private::create_runtime(window, quantum));
    }

    void Base::Materializer::release(resources::Manager manager, Reading world, Id id) const {
        auto& layer = manager->layer<Base>();
        auto& runtime = layer.value(id);
        const auto& quantum = ops::particle::get<Base>(world, id);

        GLFWwindow* window = nullptr;
        if (manager->materialized<Device>(quantum.device)) {
            window = Device::Operations::provide(world, quantum.device);
        }

        Base_private::release_runtime(window, runtime);
        layer.release(id);
    }

    void Base::Operations::bake(Reading world, Id id, resources::Manager manager) {
        ops::resource::materialize<Base>(world, manager, id);
    }

    void Base::Operations::release(Reading world, Id id, resources::Manager manager) {
        ops::resource::release<Base>(world, manager, id);
    }

    auto Base::Operations::provide(Reading world, Id id) -> RuntimeAccess {
        return world->resources->layer<Base>().provide(id);
    }

    const Invariants Base::invariants{
        .structural = {},
        .logical = {},
    };
}
