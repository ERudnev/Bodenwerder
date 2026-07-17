#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <cstddef>
#include <vector>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;
    using builders::geometry::CpuPresentation;
    using builders::geometry::GeometryGenerator;

    namespace {

        auto bake(Writing context, system::Device::Id device, const CpuPresentation& cpu) -> Runtime::Quantum {
            if (cpu.positions.empty()) {
                return context.refuse("resource::geometry::bake: positions are empty");
            }

            const auto pos_id = primitive::GeometrySemantics::id_of("position");
            const auto normal_id = primitive::GeometrySemantics::id_of("normal");
            const auto uv0_id = primitive::GeometrySemantics::id_of("uv0");

            const bool position_only = cpu.layout.size() == std::size_t{1} && cpu.layout[0] == pos_id;
            const bool position_normal = cpu.layout.size() == std::size_t{2} && cpu.layout[0] == pos_id && cpu.layout[1] == normal_id;
            const bool position_normal_uv0 =
                cpu.layout.size() == std::size_t{3}
                && cpu.layout[0] == pos_id
                && cpu.layout[1] == normal_id
                && cpu.layout[2] == uv0_id;

            if (not position_only && not position_normal && not position_normal_uv0) {
                return context.refuse("resource::geometry::bake: unsupported vertex layout");
            }

            if (position_only) {
                if (not cpu.normals.empty()) {
                    return context.refuse("resource::geometry::bake: normals must be empty for position-only layout");
                }
                if (not cpu.uv0.empty()) {
                    return context.refuse("resource::geometry::bake: uv0 must be empty for position-only layout");
                }
            } else {
                if (cpu.normals.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: normals count must match positions");
                }
                if (position_normal) {
                    if (not cpu.uv0.empty()) {
                        return context.refuse("resource::geometry::bake: uv0 must be empty for position+normal layout");
                    }
                } else if (cpu.uv0.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: uv0 count must match positions");
                }
            }

            const auto& device_quantum = with<system::Device>::get(context, device);
            glfwMakeContextCurrent(device_quantum.handle);

            const std::size_t vertex_count = cpu.positions.size();
            const bool indexed = not cpu.indices.empty();

            Runtime::VertexArray vao{};
            Runtime::VertexBuffer vbo{};
            Runtime::ElementBuffer ebo{};
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);

            if (not vao || not vbo) {
                if (vao) glDeleteVertexArrays(1, &vao);
                if (vbo) glDeleteBuffers(1, &vbo);
                return context.refuse("resource::geometry::bake: failed to allocate VAO/VBO");
            }

            std::vector<GLuint> index_data;
            if (indexed) {
                index_data.reserve(cpu.indices.size());
                for (const auto index : cpu.indices) {
                    if (index < integer{0} || static_cast<std::size_t>(index) >= vertex_count) {
                        glDeleteVertexArrays(1, &vao);
                        glDeleteBuffers(1, &vbo);
                        return context.refuse("resource::geometry::bake: index out of positions range");
                    }
                    index_data.push_back(static_cast<GLuint>(index));
                }

                glGenBuffers(1, &ebo);
                if (not ebo) {
                    glDeleteVertexArrays(1, &vao);
                    glDeleteBuffers(1, &vbo);
                    return context.refuse("resource::geometry::bake: failed to allocate EBO");
                }
            }

            std::vector<float> interleaved;

            if (position_only) {
                interleaved.reserve(vertex_count * 3);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                }

                constexpr renderer::Count stride = renderer::Count(3 * sizeof(float));
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr{0}));
                glEnableVertexAttribArray(0);
            } else if (position_normal) {
                interleaved.reserve(vertex_count * 6);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    const auto& n = cpu.normals[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(n.x);
                    interleaved.push_back(n.y);
                    interleaved.push_back(n.z);
                }

                constexpr renderer::Count stride = renderer::Count(6 * sizeof(float));
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr{0}));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
            } else {
                interleaved.reserve(vertex_count * 8);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    const auto& n = cpu.normals[i];
                    const auto& uv = cpu.uv0[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(n.x);
                    interleaved.push_back(n.y);
                    interleaved.push_back(n.z);
                    interleaved.push_back(uv.x);
                    interleaved.push_back(uv.y);
                }

                constexpr renderer::Count stride = renderer::Count(8 * sizeof(float));
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr{0}));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(6 * sizeof(float))));
                glEnableVertexAttribArray(2);
            }

            if (indexed) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, renderer::SizePtr(index_data.size() * sizeof(GLuint)), index_data.data(), GL_STATIC_DRAW);
            }

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            if (indexed) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }

            return Runtime::Quantum{
                .device = device,
                .vao = vao,
                .vbo = vbo,
                .ebo = ebo,
                .vertex_count = renderer::Count(vertex_count),
                .index_count = renderer::Count(index_data.size()),
            };
        }

        auto cpu_for(Generator::Type type) -> CpuPresentation {
            switch (type) {
                case Generator::Type::triangle: return GeometryGenerator::triangle();
                case Generator::Type::kube: return GeometryGenerator::kube();
                case Generator::Type::gridPlane: return GeometryGenerator::gridPlane();
            }
        }

    } // namespace

    auto Loader::Actions::materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum {
        _INCOMPLETE_;
    }

    auto Generator::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> Runtime::Quantum {
        const auto& generator = with<Generator>::get(context, asset_id);
        return bake(context, device, cpu_for(generator.type));
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            if (not last.vao && not last.vbo && not last.ebo) {
                return;
            }
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            if (last.vao) {
                auto vao = last.vao;
                glDeleteVertexArrays(1, &vao);
            }
            if (last.vbo) {
                auto vbo = last.vbo;
                glDeleteBuffers(1, &vbo);
            }
            if (last.ebo) {
                auto ebo = last.ebo;
                glDeleteBuffers(1, &ebo);
            }
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
