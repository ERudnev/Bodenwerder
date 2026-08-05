#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <base/logging.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <string_view>
#include <vector>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;
    using builders::geometry::CpuPresentation;
    using builders::geometry::GeometryGenerator;

    namespace {

        struct LoadedMesh {
            CpuPresentation cpu;
            umap<string, Asset::Part> parts;
            vector<Asset::Mount> slots;
        };

        // Assimp aiMatrix4x4 is row-major; glm::mat4 is column-major (RH).
        auto ai_mat4(const aiMatrix4x4& m) -> mat4 {
            return mat4{
                m.a1, m.b1, m.c1, m.d1,
                m.a2, m.b2, m.c2, m.d2,
                m.a3, m.b3, m.c3, m.d3,
                m.a4, m.b4, m.c4, m.d4,
            };
        }

        void collect_mesh_paths(
            const aiNode& node,
            vector<const aiNode*>& path,
            vector<vector<const aiNode*>>& mesh_paths)
        {
            path.push_back(&node);
            if (node.mNumMeshes > 0) {
                mesh_paths.push_back(path);
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                collect_mesh_paths(*node.mChildren[i], path, mesh_paths);
            }
            path.pop_back();
        }

        // Home = LCA of all mesh-bearing nodes (single mesh node → that node; else common ancestor, often root).
        auto home_world(const aiScene& scene) -> mat4 {
            vector<const aiNode*> scratch;
            vector<vector<const aiNode*>> mesh_paths;
            collect_mesh_paths(*scene.mRootNode, scratch, mesh_paths);
            if (mesh_paths.empty()) {
                return mat4{1.0f};
            }
            std::size_t depth = mesh_paths[0].size();
            for (std::size_t i = 1; i < mesh_paths.size(); ++i) {
                depth = std::min(depth, mesh_paths[i].size());
            }
            std::size_t lca_index = 0;
            for (; lca_index < depth; ++lca_index) {
                const aiNode* at = mesh_paths[0][lca_index];
                bool same = true;
                for (std::size_t p = 1; p < mesh_paths.size(); ++p) {
                    if (mesh_paths[p][lca_index] != at) {
                        same = false;
                        break;
                    }
                }
                if (not same) {
                    break;
                }
            }
            if (lca_index == 0) {
                return mat4{1.0f};
            }
            mat4 world{1.0f};
            for (std::size_t i = 0; i < lca_index; ++i) {
                world = world * ai_mat4(mesh_paths[0][i]->mTransformation);
            }
            return world;
        }

        void append_mesh(LoadedMesh& out, const aiMesh& mesh, const mat4& transform, const string& part_name) {
            const auto base = static_cast<integer>(out.cpu.positions.size());
            const auto normal_matrix = glm::transpose(glm::inverse(glm::mat3(transform)));
            out.cpu.positions.reserve(out.cpu.positions.size() + mesh.mNumVertices);
            out.cpu.normals.reserve(out.cpu.normals.size() + mesh.mNumVertices);
            out.cpu.uv0.reserve(out.cpu.uv0.size() + mesh.mNumVertices);
            for (unsigned i = 0; i < mesh.mNumVertices; ++i) {
                const auto& p = mesh.mVertices[i];
                const auto baked = transform * vec4{p.x, p.y, p.z, 1.0f};
                out.cpu.positions.push_back(Pos{baked.x, baked.y, baked.z});
                if (mesh.HasNormals()) {
                    const auto& n = mesh.mNormals[i];
                    const auto nw = normal_matrix * vec3{n.x, n.y, n.z};
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
                // Assimp faces are CCW in RH after import without MakeLeftHanded — keep for classic GL.
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[0]));
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[1]));
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[2]));
            }
            const auto index_count = static_cast<renderer::Count>(out.cpu.indices.size()) - index_start;
            out.parts.insert_or_assign(part_name, Asset::Part{.startIndex = index_start, .countIndex = index_count});
        }

        auto material_name(const aiScene& scene, const aiMesh& mesh) -> string {
            if (mesh.mMaterialIndex >= scene.mNumMaterials) {
                return {};
            }
            aiString name;
            if (scene.mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_NAME, name) != AI_SUCCESS or name.length == 0) {
                return {};
            }
            return name.C_Str();
        }

        auto part_name_for(const aiScene& scene, const aiMesh& mesh, std::size_t parts_so_far) -> string {
            if (const auto from_material = material_name(scene, mesh); not from_material.empty()) {
                return from_material;
            }
            return std::format("material_{}", parts_so_far);
        }

        // Meshes: bake into home space (inverse(home) * node_world). Leaf empties → slots in home space.
        void gather_node(
            LoadedMesh& out,
            const aiScene& scene,
            const aiNode& node,
            const mat4& parent_world,
            const mat4& home_inverse)
        {
            const auto world = parent_world * ai_mat4(node.mTransformation);
            const auto in_home = home_inverse * world;

            if (node.mNumMeshes == 0 and node.mNumChildren == 0 and node.mName.length > 0) {
                out.slots.push_back(Asset::Mount{.name = node.mName.C_Str(), .transform = in_home});
            }

            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                const auto* mesh = scene.mMeshes[node.mMeshes[i]];
                if (not mesh or mesh->mNumVertices == 0) {
                    continue;
                }
                auto part_name = part_name_for(scene, *mesh, out.parts.size());
                if (out.parts.find(part_name) != out.parts.end()) {
                    part_name = std::format("{}_{}", part_name, out.parts.size());
                }
                append_mesh(out, *mesh, in_home, part_name);
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                gather_node(out, scene, *node.mChildren[i], world, home_inverse);
            }
        }

        // Collect only this node's meshes (no recursion) — one LWO layer leaf.
        void gather_node_meshes_only(
            LoadedMesh& out,
            const aiScene& scene,
            const aiNode& node,
            const mat4& world,
            const mat4& home_inverse)
        {
            const auto in_home = home_inverse * world;
            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                const auto* mesh = scene.mMeshes[node.mMeshes[i]];
                if (not mesh or mesh->mNumVertices == 0) {
                    continue;
                }
                auto part_name = part_name_for(scene, *mesh, out.parts.size());
                if (out.parts.find(part_name) != out.parts.end()) {
                    part_name = std::format("{}_{}", part_name, out.parts.size());
                }
                append_mesh(out, *mesh, in_home, part_name);
            }
        }

        auto layer_identity(std::string_view raw) -> string {
            const auto eq = raw.find('=');
            if (eq == std::string_view::npos or eq == 0) {
                return string(raw);
            }
            return string(raw.substr(0, eq));
        }

        auto find_node(const aiNode& node, const string& name) -> const aiNode* {
            if (node.mName.C_Str() == name or layer_identity(node.mName.C_Str()) == name) {
                return &node;
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                if (const auto* found = find_node(*node.mChildren[i], name)) {
                    return found;
                }
            }
            return nullptr;
        }

        auto node_world(const aiNode& root, const aiNode& target) -> optional<mat4> {
            // Path root → target by search; accumulate transforms.
            struct Frame {
                const aiNode* node;
                mat4 world;
            };
            vector<Frame> stack{{&root, ai_mat4(root.mTransformation)}};
            while (not stack.empty()) {
                const auto frame = stack.back();
                stack.pop_back();
                if (frame.node == &target) {
                    return frame.world;
                }
                for (unsigned i = 0; i < frame.node->mNumChildren; ++i) {
                    const auto* child = frame.node->mChildren[i];
                    stack.push_back({child, frame.world * ai_mat4(child->mTransformation)});
                }
            }
            return {};
        }

        auto read_assimp_scene(Assimp::Importer& importer, const filepath& path) -> const aiScene* {
            // RH OpenGL: do not MakeLeftHanded / FlipWindingOrder.
            // Keep file normals (LW OBJ hard/smooth). GenSmoothNormals only if a mesh has none — and before Join (Assimp needs verbose verts).
            const auto* scene = importer.ReadFile(
                path.string(),
                aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_FlipUVs);
            if (not scene or not scene->mRootNode or (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) or scene->mNumMeshes == 0) {
                return nullptr;
            }
            bool missing_normals = false;
            for (unsigned mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
                const auto* mesh = scene->mMeshes[mesh_index];
                if (mesh and mesh->mNumVertices > 0 and not mesh->HasNormals()) {
                    missing_normals = true;
                    break;
                }
            }
            if (missing_normals) {
                scene = importer.ApplyPostProcessing(aiProcess_GenSmoothNormals);
                if (not scene) {
                    return nullptr;
                }
            }
            scene = importer.ApplyPostProcessing(aiProcess_JoinIdenticalVertices);
            return scene;
        }

        auto load_assimp(const filepath& path, const string& layer) -> optional<LoadedMesh> {
            Assimp::Importer importer;
            const auto* scene = read_assimp_scene(importer, path);
            if (not scene) {
                return {};
            }

            LoadedMesh out{};
            out.cpu.layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"});

            if (layer.empty()) {
                const mat4 home = home_world(*scene);
                const mat4 home_inverse = glm::inverse(home);
                gather_node(out, *scene, *scene->mRootNode, mat4{1.0f}, home_inverse);
            } else {
                const auto* target = find_node(*scene->mRootNode, layer);
                if (not target) {
                    return {};
                }
                const auto world = node_world(*scene->mRootNode, *target);
                if (not world) {
                    return {};
                }
                const mat4 home_inverse = glm::inverse(*world);
                gather_node_meshes_only(out, *scene, *target, *world, home_inverse);
            }

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
        const auto path = with<Manager>::resolve(context, unit, loader.file);
        base::whisper("rmmr: geometry::Loader '{}' ← {}{}", unit.name.text(), path.string(), loader.layer.empty() ? "" : std::format(" layer '{}'", loader.layer));

        const auto loaded = load_assimp(path, loader.layer);
        if (not loaded) {
            return context.refuse(std::format(
                "resource::geometry::Loader::materialize: Assimp failed '{}' layer '{}'",
                path.string(),
                loader.layer.empty() ? "(whole)" : loader.layer));
        }

        {
            auto asset = with<Asset>::modify(context, asset_id);
            asset->parts = loaded->parts;
            asset->slots = loaded->slots;
        }
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
