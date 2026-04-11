#include <Raidenmamare/primitives/base.q1.h>

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace rmmr::primitive {
    struct Base_private : Base::Operations {
        using Passport = Base::Materializer::Passport;
        using Runtime = Base::RuntimeStorage;

        static auto create_runtime(GLFWwindow* window, const Quantum& quantum) -> Runtime {
            if (!window) {
                throw std::runtime_error("primitive::Base::Materializer::materialize: device is not open");
            }

            if (quantum.vertices.empty()) {
                throw std::runtime_error("primitive::Base::Materializer::materialize: vertices are empty");
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

            glBindVertexArray(runtime.vao);
            glBindBuffer(GL_ARRAY_BUFFER, runtime.vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(quantum.vertices.size() * sizeof(vec3)),
                quantum.vertices.data(),
                GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
            glEnableVertexAttribArray(0);

            runtime.vertex_count = static_cast<integer>(quantum.vertices.size());
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
        GLFWwindow* window = Device::Operations::provide(world, quantum.device, manager);
        manager->layer<Base>().materialize(id, Base_private::create_runtime(window, quantum));
    }

    void Base::Materializer::release(resources::Manager manager, Reading world, Id id) const {
        auto& layer = manager->layer<Base>();
        auto& runtime = layer.value(id);
        const auto& quantum = ops::particle::get<Base>(world, id);

        GLFWwindow* window = nullptr;
        if (manager->materialized<Device>(quantum.device)) {
            window = Device::Operations::provide(world, quantum.device, manager);
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

    auto Base::Operations::provide(Reading, Id id, resources::Manager manager) -> RuntimeAccess {
        return manager->layer<Base>().provide(id);
    }

    const Invariants Base::invariants{
        .structural = {},
        .logical = {},
    };
}
