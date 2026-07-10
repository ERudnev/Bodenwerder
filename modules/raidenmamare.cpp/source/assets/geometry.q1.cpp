#include <Raidenmamare/assets/geometry.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace rmmr::asset {

    using namespace fqsm::api;

    namespace {

        auto create_resource_quantum(Writing context, const Geometry::Quantum& asset, system::Device::Id device) -> resource::Geometry::Quantum {
            if (asset.positions.empty()) {
                throw std::runtime_error("asset::Geometry::compile: positions are empty");
            }

            const auto pos_id = primitive::GeometrySemantics::id_of("position");
            const auto normal_id = primitive::GeometrySemantics::id_of("normal");

            const bool position_only = asset.layout.size() == std::size_t{1} && asset.layout[0] == pos_id;
            const bool position_normal = asset.layout.size() == std::size_t{2} && asset.layout[0] == pos_id && asset.layout[1] == normal_id;

            if (not position_only && not position_normal) {
                throw std::runtime_error("asset::Geometry::compile: unsupported vertex layout (expect position only, or position+normal)");
            }

            if (position_only) {
                if (not asset.normals.empty()) {
                    throw std::runtime_error("asset::Geometry::compile: normals must be empty for position-only layout");
                }
            } else if (asset.normals.size() != asset.positions.size()) {
                throw std::runtime_error("asset::Geometry::compile: normals count must match positions");
            }

            const auto& device_quantum = with<system::Device>::get(context, device);
            glfwMakeContextCurrent(device_quantum.handle);

            resource::Geometry::VertexArray vao{};
            resource::Geometry::VertexBuffer vbo{};
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);

            if (not vao || not vbo) {
                if (vao) glDeleteVertexArrays(1, &vao);
                if (vbo) glDeleteBuffers(1, &vbo);
                throw std::runtime_error("asset::Geometry::compile: failed to allocate VAO/VBO");
            }

            const std::size_t vertex_count = asset.positions.size();
            std::vector<float> interleaved;

            if (position_only) {
                interleaved.reserve(vertex_count * 3);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = asset.positions[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                }

                constexpr GLsizei stride = static_cast<GLsizei>(3 * sizeof(float));

                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0)));
                glEnableVertexAttribArray(0);
            } else {
                interleaved.reserve(vertex_count * 6);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = asset.positions[i];
                    const auto& n = asset.normals[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(n.x);
                    interleaved.push_back(n.y);
                    interleaved.push_back(n.z);
                }

                constexpr GLsizei stride = static_cast<GLsizei>(6 * sizeof(float));

                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(static_cast<std::uintptr_t>(0)));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(static_cast<std::uintptr_t>(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
            }

            return resource::Geometry::Quantum{
                .vao = vao,
                .vbo = vbo,
                .vertex_count = static_cast<integer>(vertex_count),
            };
        }

    } // namespace

    auto Geometry::Always::layoutIds(const vector<string>& names) -> Channel::Layout {
        Channel::Layout out;
        out.reserve(names.size());

        for (const auto& name : names) {
            const auto id = primitive::GeometrySemantics::id_of(name);
            if (id == primitive::GeometrySemantics::PersistentId{0}) {
                throw std::runtime_error("asset::Geometry::layoutIds: unknown geometry channel semantic: " + name);
            }
            out.push_back(id);
        }

        return out;
    }

    auto Geometry::Actions::compile(Writing context, Id asset_geometry, system::Device::Id device) -> resource::Geometry::Id {
        const auto& asset = with<Geometry>::get(context, asset_geometry);
        with<resource::Geometry_group>::extend(context, device);
        return with<resource::Geometry_group>::addElement(context, device, create_resource_quantum(context, asset, device));
    }

}
