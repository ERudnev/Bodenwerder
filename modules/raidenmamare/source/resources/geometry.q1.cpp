#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <base/logging.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <cstddef>
#include <filesystem>
#include <format>
#include <vector>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;
    using builders::geometry::CpuPresentation;
    using builders::geometry::GeometryGenerator;

    namespace {

        auto resolve_under_manager(const Manager::Quantum& manager, const Unit::Quantum& unit, const filename& relative) -> filepath {
            const std::filesystem::path file_path(relative);
            if (file_path.is_absolute()) {
                return file_path;
            }
            if (unit.library.empty()) {
                return manager.location / file_path;
            }
            return manager.location / unit.library / file_path;
        }

        struct LoadedMesh {
            CpuPresentation cpu;
            umap<string, Asset::Part> parts;
        };

        auto ai_mat4(const aiMatrix4x4& m) -> glm::mat4 {
            return glm::mat4{
                m.a1, m.b1, m.c1, m.d1,
                m.a2, m.b2, m.c2, m.d2,
                m.a3, m.b3, m.c3, m.d3,
                m.a4, m.b4, m.c4, m.d4,
            };
        }

        void append_mesh(LoadedMesh& out, const aiMesh& mesh, const glm::mat4& transform, const string& part_name) {
            const auto base = static_cast<integer>(out.cpu.positions.size());
            const auto normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));
            out.cpu.positions.reserve(out.cpu.positions.size() + mesh.mNumVertices);
            out.cpu.normals.reserve(out.cpu.normals.size() + mesh.mNumVertices);
            out.cpu.uv0.reserve(out.cpu.uv0.size() + mesh.mNumVertices);
            for (unsigned i = 0; i < mesh.mNumVertices; ++i) {
                const auto& p = mesh.mVertices[i];
                const auto world = transform * glm::vec4{p.x, p.y, p.z, 1.0f};
                out.cpu.positions.push_back(Pos{world.x, world.y, world.z});
                if (mesh.HasNormals()) {
                    const auto& n = mesh.mNormals[i];
                    const auto nw = normal_matrix * glm::vec3{n.x, n.y, n.z};
                    out.cpu.normals.push_back(Pos{nw.x, nw.y, nw.z});
                } else {
                    out.cpu.normals.push_back(Pos{0.0f, 0.0f, 1.0f});
                }
                if (mesh.HasTextureCoords(0)) {
                    const auto& uv = mesh.mTextureCoords[0][i];
                    out.cpu.uv0.push_back(UV{uv.x, uv.y});
                } else {
                    out.cpu.uv0.push_back(UV{0.0f, 0.0f});
                }
            }

            const auto index_start = static_cast<renderer::Count>(out.cpu.indices.size());
            out.cpu.indices.reserve(out.cpu.indices.size() + mesh.mNumFaces * 3);
            for (unsigned f = 0; f < mesh.mNumFaces; ++f) {
                const auto& face = mesh.mFaces[f];
                if (face.mNumIndices != 3) {
                    continue;
                }
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[0]));
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[1]));
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[2]));
            }
            const auto index_count = static_cast<renderer::Count>(out.cpu.indices.size()) - index_start;
            out.parts.insert_or_assign(part_name, Asset::Part{.startIndex = index_start, .countIndex = index_count});
        }

        void gather_node(LoadedMesh& out, const aiScene& scene, const aiNode& node, const glm::mat4& parent, bool single_mesh) {
            const auto transform = parent * ai_mat4(node.mTransformation);
            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                const auto* mesh = scene.mMeshes[node.mMeshes[i]];
                if (not mesh or mesh->mNumVertices == 0) {
                    continue;
                }
                string part_name;
                if (single_mesh) {
                    part_name = "mesh";
                } else if (mesh->mName.length > 0) {
                    part_name = mesh->mName.C_Str();
                } else if (node.mName.length > 0) {
                    part_name = node.mName.C_Str();
                } else {
                    part_name = std::format("part_{}", out.parts.size());
                }
                if (out.parts.find(part_name) != out.parts.end()) {
                    part_name = std::format("{}_{}", part_name, out.parts.size());
                }
                append_mesh(out, *mesh, transform, part_name);
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                gather_node(out, scene, *node.mChildren[i], transform, single_mesh);
            }
        }

        auto load_assimp(const filepath& path) -> optional<LoadedMesh> {
            Assimp::Importer importer;
            const auto* scene = importer.ReadFile(
                path.string(),
                aiProcess_Triangulate
                    | aiProcess_GenSmoothNormals
                    | aiProcess_JoinIdenticalVertices
                    | aiProcess_SortByPType
                    | aiProcess_FlipUVs);
            if (not scene or not scene->mRootNode or (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) or scene->mNumMeshes == 0) {
                return {};
            }
            LoadedMesh out{};
            out.cpu.layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"});
            const bool single_mesh = scene->mNumMeshes == 1;
            gather_node(out, *scene, *scene->mRootNode, glm::mat4{1.0f}, single_mesh);
            if (out.cpu.positions.empty() or out.cpu.indices.empty()) {
                return {};
            }
            return out;
        }

        auto bake(Writing context, system::Device::Id device, const CpuPresentation& cpu) -> Runtime::Quantum {
            if (cpu.positions.empty()) {
                return context.refuse("resource::geometry::bake: positions are empty");
            }

            const auto pos_id = primitive::GeometrySemantics::id_of("position");
            const auto normal_id = primitive::GeometrySemantics::id_of("normal");
            const auto uv0_id = primitive::GeometrySemantics::id_of("uv0");
            const auto color0_id = primitive::GeometrySemantics::id_of("color0");

            const bool position_only = cpu.layout.size() == std::size_t{1} && cpu.layout[0] == pos_id;
            const bool position_normal = cpu.layout.size() == std::size_t{2} && cpu.layout[0] == pos_id && cpu.layout[1] == normal_id;
            const bool position_uv0 = cpu.layout.size() == std::size_t{2} && cpu.layout[0] == pos_id && cpu.layout[1] == uv0_id;
            const bool position_color0 = cpu.layout.size() == std::size_t{2} && cpu.layout[0] == pos_id && cpu.layout[1] == color0_id;
            const bool position_uv0_color0 =
                cpu.layout.size() == std::size_t{3}
                && cpu.layout[0] == pos_id
                && cpu.layout[1] == uv0_id
                && cpu.layout[2] == color0_id;
            const bool position_normal_uv0 =
                cpu.layout.size() == std::size_t{3}
                && cpu.layout[0] == pos_id
                && cpu.layout[1] == normal_id
                && cpu.layout[2] == uv0_id;

            if (not position_only && not position_normal && not position_uv0 && not position_color0 && not position_uv0_color0 && not position_normal_uv0) {
                return context.refuse("resource::geometry::bake: unsupported vertex layout");
            }

            if (position_only) {
                if (not cpu.normals.empty()) {
                    return context.refuse("resource::geometry::bake: normals must be empty for position-only layout");
                }
                if (not cpu.uv0.empty()) {
                    return context.refuse("resource::geometry::bake: uv0 must be empty for position-only layout");
                }
                if (not cpu.color0.empty()) {
                    return context.refuse("resource::geometry::bake: color0 must be empty for position-only layout");
                }
            } else if (position_uv0) {
                if (not cpu.normals.empty()) {
                    return context.refuse("resource::geometry::bake: normals must be empty for position+uv0 layout");
                }
                if (not cpu.color0.empty()) {
                    return context.refuse("resource::geometry::bake: color0 must be empty for position+uv0 layout");
                }
                if (cpu.uv0.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: uv0 count must match positions");
                }
            } else if (position_color0) {
                if (not cpu.normals.empty()) {
                    return context.refuse("resource::geometry::bake: normals must be empty for position+color0 layout");
                }
                if (not cpu.uv0.empty()) {
                    return context.refuse("resource::geometry::bake: uv0 must be empty for position+color0 layout");
                }
                if (cpu.color0.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: color0 count must match positions");
                }
            } else if (position_uv0_color0) {
                if (not cpu.normals.empty()) {
                    return context.refuse("resource::geometry::bake: normals must be empty for position+uv0+color0 layout");
                }
                if (cpu.uv0.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: uv0 count must match positions");
                }
                if (cpu.color0.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: color0 count must match positions");
                }
            } else {
                if (cpu.normals.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: normals count must match positions");
                }
                if (not cpu.color0.empty()) {
                    return context.refuse("resource::geometry::bake: color0 must be empty for this layout");
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

            renderer::VertexArray vao{};
            renderer::VertexBuffer vbo{};
            renderer::ElementBuffer ebo{};
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
                    if (index < 0 || static_cast<std::size_t>(index) >= vertex_count) {
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
            } else if (position_uv0) {
                interleaved.reserve(vertex_count * 5);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    const auto& uv = cpu.uv0[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(uv.x);
                    interleaved.push_back(uv.y);
                }

                constexpr renderer::Count stride = renderer::Count(5 * sizeof(float));
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr{0}));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
            } else if (position_color0) {
                interleaved.reserve(vertex_count * 7);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    const auto& color = cpu.color0[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(color.x);
                    interleaved.push_back(color.y);
                    interleaved.push_back(color.z);
                    interleaved.push_back(color.w);
                }

                constexpr renderer::Count stride = renderer::Count(7 * sizeof(float));
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr{0}));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
            } else if (position_uv0_color0) {
                interleaved.reserve(vertex_count * 9);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    const auto& uv = cpu.uv0[i];
                    const auto& color = cpu.color0[i];
                    interleaved.push_back(p.x);
                    interleaved.push_back(p.y);
                    interleaved.push_back(p.z);
                    interleaved.push_back(uv.x);
                    interleaved.push_back(uv.y);
                    interleaved.push_back(color.x);
                    interleaved.push_back(color.y);
                    interleaved.push_back(color.z);
                    interleaved.push_back(color.w);
                }

                constexpr renderer::Count stride = renderer::Count(9 * sizeof(float));
                glBindVertexArray(vao);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glBufferData(GL_ARRAY_BUFFER, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr{0}));
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(3 * sizeof(float))));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(renderer::IntPtr(5 * sizeof(float))));
                glEnableVertexAttribArray(2);
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
                case Generator::Type::windowedKube: return GeometryGenerator::windowedKube();
                case Generator::Type::bagel: return GeometryGenerator::bagel();
                case Generator::Type::gridPlane: return GeometryGenerator::gridPlane();
                case Generator::Type::unitQuad: return GeometryGenerator::unitQuad();
                case Generator::Type::sphere: return GeometryGenerator::sphere();
                case Generator::Type::diamond: return GeometryGenerator::diamond();
            }
        }

        void release_gl(Writing context, const Runtime::Quantum& last) {
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

        auto install_runtime(Writing context, system::Device::Id device, Asset::Id asset_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.geometries_id_mapping.find(asset_id); existing != runtimes.geometries_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<GeometryRuntime_group>::addElement(context, device, std::move(quantum));
        }

    } // namespace

    auto Asset::Actions::install(Writing context, Id asset_id, system::Device::Id device, const CpuPresentation& cpu) -> optional<Runtime::Id> {
        auto quantum = bake(context, device, cpu);
        if (not quantum.vao) {
            return {};
        }
        const auto runtime_id = install_runtime(context, device, asset_id, std::move(quantum));
        with<Runtimes>::modify(context, device)->geometries_id_mapping.insert_or_assign(asset_id, runtime_id);
        return runtime_id;
    }

    auto Loader::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& loader = with<Loader>::get(context, asset_id);
        const auto& unit = with<Unit>::get(context, asset_id);
        const auto manager_id = with<Manager>::singleton(context);
        if (not manager_id) {
            return context.refuse("resource::geometry::Loader::materialize: Manager singleton missing");
        }
        const auto path = resolve_under_manager(with<Manager>::get(context, *manager_id), unit, loader.file);
        base::whisper("rmmr: geometry::Loader '{}/{}' ← {}", unit.library, unit.name, path.string());

        const auto loaded = load_assimp(path);
        if (not loaded) {
            return context.refuse(std::format("resource::geometry::Loader::materialize: Assimp failed '{}'", path.string()));
        }

        with<Asset>::modify(context, asset_id)->parts = loaded->parts;
        return Asset::Actions::install(context, asset_id, device, loaded->cpu);
    }

    auto Generator::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& generator = with<Generator>::get(context, asset_id);
        auto quantum = bake(context, device, cpu_for(generator.type));
        if (not quantum.vao) {
            return {};
        }
        return install_runtime(context, device, asset_id, std::move(quantum));
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            release_gl(context, last);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
