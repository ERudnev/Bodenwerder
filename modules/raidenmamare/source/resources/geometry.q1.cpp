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

#include <glm/common.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <vector>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;
    using builders::geometry::CpuPresentation;
    using builders::geometry::GeometryGenerator;

    namespace {

        struct LoadedMesh {
            CpuPresentation cpu;
            vector<Asset::Entry> entries;
            vector<Asset::Surface> surfaces;
            vector<Asset::Mount> mounts;
            umap<string, EntryId> entryCatalog;
            vector<umap<string, SurfaceId>> surfaceCatalogs;
            vector<SurfaceId> primitiveSurfaces;
        };

        auto material_name(const aiScene& scene, const aiMesh& mesh) -> string;

        void append_mesh(LoadedMesh& out, const aiMesh& mesh, const mat4& transform, umap<string, SurfaceId>& catalog, const aiScene& scene) {
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

            const auto indexStart = static_cast<renderer::Count>(out.cpu.indices.size());
            const auto surface = static_cast<SurfaceId>(out.surfaces.size());
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
                out.primitiveSurfaces.push_back(surface);
            }
            const auto indexCount = static_cast<renderer::Count>(out.cpu.indices.size()) - indexStart;
            out.surfaces.push_back(Asset::Surface{.indices = Asset::Range{.first = indexStart, .count = indexCount}});
            auto surfaceName = material_name(scene, mesh);
            if (surfaceName.empty()) surfaceName = std::format("surface_{}", catalog.size());
            if (catalog.contains(surfaceName)) surfaceName = std::format("{}_{}", surfaceName, catalog.size());
            catalog.emplace(std::move(surfaceName), surface);
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

        void collectEntryNodes(const aiNode& node, vector<const aiNode*>& nodes) {
            if (node.mNumMeshes > 0) nodes.push_back(&node);
            for (unsigned i = 0; i < node.mNumChildren; ++i) collectEntryNodes(*node.mChildren[i], nodes);
        }

        auto aiTranslation(const aiMatrix4x4& m) -> vec3 {
            return vec3{m.a4, m.b4, m.c4};
        }

        // Assimp LWO: Pivot-* parent holds +pivot; mesh node holds -pivot. Same space as mesh verts (post MakeLeftHanded).
        auto extractOrigin(const aiNode& node) -> vec3 {
            if (node.mParent) {
                const string parentName{node.mParent->mName.C_Str()};
                if (parentName.starts_with("Pivot-"))
                    return aiTranslation(node.mParent->mTransformation);
            }
            return -aiTranslation(node.mTransformation);
        }

        void bakeOriginIntoVertices(LoadedMesh& out, renderer::Count firstVertex, const vec3& origin) {
            for (auto i = static_cast<std::size_t>(firstVertex); i < out.cpu.positions.size(); ++i) {
                out.cpu.positions[i].x -= origin.x;
                out.cpu.positions[i].y -= origin.y;
                out.cpu.positions[i].z -= origin.z;
            }
        }

        void appendEntry(LoadedMesh& out, const aiScene& scene, const aiNode& node) {
            const auto firstVertex = static_cast<renderer::Count>(out.cpu.positions.size());
            const auto firstIndex = static_cast<renderer::Count>(out.cpu.indices.size());
            const auto firstSurface = static_cast<renderer::Count>(out.surfaces.size());
            umap<string, SurfaceId> catalog;
            vector<const aiMesh*> meshes;
            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                const auto* mesh = scene.mMeshes[node.mMeshes[i]];
                if (mesh and mesh->mNumVertices > 0) meshes.push_back(mesh);
            }
            std::ranges::sort(meshes, [&](const aiMesh* left, const aiMesh* right) { return material_name(scene, *left) < material_name(scene, *right); });
            for (const auto* mesh : meshes) append_mesh(out, *mesh, mat4{1.0f}, catalog, scene);
            if (static_cast<renderer::Count>(out.cpu.indices.size()) == firstIndex) return;
            const auto origin = extractOrigin(node);
            bakeOriginIntoVertices(out, firstVertex, origin);
            const auto entry = static_cast<EntryId>(out.entries.size());
            auto name = string{node.mName.C_Str()};
            if (name.empty()) name = std::format("entry_{}", entry);
            if (out.entryCatalog.contains(name)) name = std::format("{}_{}", name, entry);
            out.entryCatalog.emplace(std::move(name), entry);
            out.entries.push_back(Asset::Entry{
                .vertices = Asset::Range{.first = firstVertex, .count = static_cast<renderer::Count>(out.cpu.positions.size()) - firstVertex},
                .indices = Asset::Range{.first = firstIndex, .count = static_cast<renderer::Count>(out.cpu.indices.size()) - firstIndex},
                .surfaces = Asset::Range{.first = firstSurface, .count = static_cast<renderer::Count>(out.surfaces.size()) - firstSurface},
                .mounts = Asset::Range{.first = static_cast<renderer::Count>(out.mounts.size()), .count = 0},
                .origin = origin,
            });
            out.surfaceCatalogs.push_back(std::move(catalog));
        }

        void appendWhole(LoadedMesh& out, const aiScene& scene, string name) {
            const auto firstIndex = static_cast<renderer::Count>(out.cpu.indices.size());
            umap<string, SurfaceId> catalog;
            vector<const aiMesh*> meshes;
            for (unsigned index = 0; index < scene.mNumMeshes; ++index) {
                const auto* mesh = scene.mMeshes[index];
                if (mesh and mesh->mNumVertices > 0) meshes.push_back(mesh);
            }
            std::ranges::sort(meshes, [&](const aiMesh* left, const aiMesh* right) { return material_name(scene, *left) < material_name(scene, *right); });
            for (const auto* mesh : meshes) append_mesh(out, *mesh, mat4{1.0f}, catalog, scene);
            const auto entry = static_cast<EntryId>(out.entries.size());
            out.entryCatalog.emplace(std::move(name), entry);
            out.entries.push_back(Asset::Entry{
                .vertices = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(out.cpu.positions.size())},
                .indices = Asset::Range{.first = firstIndex, .count = static_cast<renderer::Count>(out.cpu.indices.size()) - firstIndex},
                .surfaces = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(out.surfaces.size())},
                .mounts = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}},
                .origin = vec3{0.0f, 0.0f, 0.0f},
            });
            out.surfaceCatalogs.push_back(std::move(catalog));
        }

        auto read_assimp_scene(Assimp::Importer& importer, const filepath& path) -> const aiScene* {
            // RH OpenGL: do not pass MakeLeftHanded / FlipWindingOrder here.
            // Note: Assimp LWOImporter still applies both inside GenerateNodeGraph — undone in load_assimp via restore_lwo_file_space.
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

        // Assimp LWOImporter::GenerateNodeGraph always MakeLeftHanded (z*=-1) then FlipWindingOrder
        // (hardcoded; ReadFile flags cannot disable it). Restore file/RH/RedStar space for our pipeline.
        void restore_lwo_file_space(LoadedMesh& mesh) {
            for (auto& p : mesh.cpu.positions)
                p.z = -p.z;
            for (auto& n : mesh.cpu.normals)
                n.z = -n.z;
            for (auto& entry : mesh.entries)
                entry.origin.z = -entry.origin.z;
            auto& indices = mesh.cpu.indices;
            for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
                std::swap(indices[i + 1], indices[i + 2]);
        }

        auto load_assimp(const filepath& path) -> optional<LoadedMesh> {
            Assimp::Importer importer;
            const auto* scene = read_assimp_scene(importer, path);
            if (not scene) {
                return {};
            }

            LoadedMesh out{};
            out.cpu.layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"});

            const auto extension = path.extension().string();
            const bool lwo = extension == ".lwo" or extension == ".LWO";
            if (lwo) {
                vector<const aiNode*> nodes;
                collectEntryNodes(*scene->mRootNode, nodes);
                std::ranges::sort(nodes, [](const aiNode* left, const aiNode* right) { return string{left->mName.C_Str()} < string{right->mName.C_Str()}; });
                for (const auto* node : nodes) appendEntry(out, *scene, *node);
            } else {
                appendWhole(out, *scene, path.stem().string());
            }

            if (out.cpu.positions.empty() or out.cpu.indices.empty()) {
                return {};
            }

            if (lwo) restore_lwo_file_space(out);

            return out;
        }

        auto bake(Writing context, system::Device::Id device, const CpuPresentation& cpu, const vector<SurfaceId>& sourcePrimitiveSurfaces) -> Runtime::Quantum {
            if (cpu.positions.empty()) {
                return context.refuse("resource::geometry::bake: positions are empty");
            }

            const auto pos_id = primitive::GeometrySemantics::id_of("position");
            const auto normal_id = primitive::GeometrySemantics::id_of("normal");
            const auto uv0_id = primitive::GeometrySemantics::id_of("uv0");
            const auto color0_id = primitive::GeometrySemantics::id_of("color0");
            const auto mix0_id = primitive::GeometrySemantics::id_of("mix0");
            const auto cohesion_id = primitive::GeometrySemantics::id_of("cohesion");

            auto attribs = cpu.layout;
            const bool liveCohesion = not attribs.empty() and attribs.back() == cohesion_id;
            const GLuint cohesionLocation = liveCohesion ? static_cast<GLuint>(attribs.size() - 1) : 0;
            if (liveCohesion)
                attribs.pop_back();

            const bool position_only = attribs.size() == std::size_t{1} && attribs[0] == pos_id;
            const bool position_normal = attribs.size() == std::size_t{2} && attribs[0] == pos_id && attribs[1] == normal_id;
            const bool position_uv0 = attribs.size() == std::size_t{2} && attribs[0] == pos_id && attribs[1] == uv0_id;
            const bool position_color0 = attribs.size() == std::size_t{2} && attribs[0] == pos_id && attribs[1] == color0_id;
            const bool position_uv0_color0 =
                attribs.size() == std::size_t{3}
                && attribs[0] == pos_id
                && attribs[1] == uv0_id
                && attribs[2] == color0_id;
            const bool position_normal_uv0 =
                attribs.size() == std::size_t{3}
                && attribs[0] == pos_id
                && attribs[1] == normal_id
                && attribs[2] == uv0_id;
            const bool position_normal_mix0 =
                attribs.size() == std::size_t{3}
                && attribs[0] == pos_id
                && attribs[1] == normal_id
                && attribs[2] == mix0_id;

            if (not position_only && not position_normal && not position_uv0 && not position_color0 && not position_uv0_color0 && not position_normal_uv0 && not position_normal_mix0) {
                return context.refuse("resource::geometry::bake: unsupported vertex layout");
            }
            if (not position_normal_mix0 and not cpu.mix0.empty()) {
                return context.refuse("resource::geometry::bake: mix0 must be empty for this layout");
            }
            if (liveCohesion and cpu.cohesion.size() != cpu.positions.size()) {
                return context.refuse("resource::geometry::bake: cohesion count must match positions");
            }
            if (not liveCohesion and not cpu.cohesion.empty()) {
                return context.refuse("resource::geometry::bake: cohesion must be empty for this layout");
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
            } else if (position_normal_mix0) {
                if (cpu.normals.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: normals count must match positions");
                }
                if (cpu.mix0.size() != cpu.positions.size()) {
                    return context.refuse("resource::geometry::bake: mix0 count must match positions");
                }
                if (not cpu.uv0.empty()) {
                    return context.refuse("resource::geometry::bake: uv0 must be empty for this layout");
                }
                if (not cpu.color0.empty()) {
                    return context.refuse("resource::geometry::bake: color0 must be empty for this layout");
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
            const bool sourceIndexed = not cpu.indices.empty();
            const bool indexed = true;

            renderer::VertexArray vao{};
            renderer::VertexBuffer vbo{};
            renderer::ElementBuffer ebo{};
            renderer::StorageBuffer primitiveSurfaces{};
            glCreateVertexArrays(1, &vao);
            glCreateBuffers(1, &vbo);
            glCreateBuffers(1, &primitiveSurfaces);

            if (not vao || not vbo || not primitiveSurfaces) {
                if (vao) glDeleteVertexArrays(1, &vao);
                if (vbo) glDeleteBuffers(1, &vbo);
                if (primitiveSurfaces) glDeleteBuffers(1, &primitiveSurfaces);
                return context.refuse("resource::geometry::bake: failed to allocate VAO/VBO/primitive-surface SSBO");
            }

            std::vector<GLuint> index_data;
            if (sourceIndexed) {
                index_data.reserve(cpu.indices.size());
                for (const auto index : cpu.indices) {
                    if (index < 0 || static_cast<std::size_t>(index) >= vertex_count) {
                        glDeleteVertexArrays(1, &vao);
                        glDeleteBuffers(1, &vbo);
                        glDeleteBuffers(1, &primitiveSurfaces);
                        return context.refuse("resource::geometry::bake: index out of positions range");
                    }
                    index_data.push_back(static_cast<GLuint>(index));
                }

            } else {
                index_data.reserve(vertex_count);
                for (std::size_t index = 0; index < vertex_count; ++index) index_data.push_back(static_cast<GLuint>(index));
            }
            glCreateBuffers(1, &ebo);
            if (not ebo) {
                glDeleteVertexArrays(1, &vao);
                glDeleteBuffers(1, &vbo);
                glDeleteBuffers(1, &primitiveSurfaces);
                return context.refuse("resource::geometry::bake: failed to allocate EBO");
            }

            auto setupAttrib = [&](GLuint index, GLint components, GLuint relativeOffset) {
                glEnableVertexArrayAttrib(vao, index);
                glVertexArrayAttribFormat(vao, index, components, GL_FLOAT, GL_FALSE, relativeOffset);
                glVertexArrayAttribBinding(vao, index, 0);
            };
            auto setupAttribI = [&](GLuint index, GLint components, GLenum type, GLuint relativeOffset) {
                glEnableVertexArrayAttrib(vao, index);
                glVertexArrayAttribIFormat(vao, index, components, type, relativeOffset);
                glVertexArrayAttribBinding(vao, index, 0);
            };

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
                glNamedBufferData(vbo, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
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
                glNamedBufferData(vbo, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
                setupAttrib(1, 2, renderer::Count(3 * sizeof(float)));
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
                glNamedBufferData(vbo, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
                setupAttrib(1, 4, renderer::Count(3 * sizeof(float)));
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
                glNamedBufferData(vbo, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
                setupAttrib(1, 2, renderer::Count(3 * sizeof(float)));
                setupAttrib(2, 4, renderer::Count(5 * sizeof(float)));
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
                glNamedBufferData(vbo, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
                setupAttrib(1, 3, renderer::Count(3 * sizeof(float)));
            } else if (position_normal_mix0) {
                struct Packed {
                    float px;
                    float py;
                    float pz;
                    float nx;
                    float ny;
                    float nz;
                    std::uint32_t mixLo;
                    std::uint32_t mixHi;
                };
                static_assert(sizeof(Packed) == 32);
                std::vector<Packed> packed;
                packed.reserve(vertex_count);
                for (std::size_t i = 0; i < vertex_count; ++i) {
                    const auto& p = cpu.positions[i];
                    const auto& n = cpu.normals[i];
                    packed.push_back(Packed{.px = p.x, .py = p.y, .pz = p.z, .nx = n.x, .ny = n.y, .nz = n.z, .mixLo = static_cast<std::uint32_t>(cpu.mix0[i]), .mixHi = static_cast<std::uint32_t>(cpu.mix0[i] >> 32)});
                }
                constexpr renderer::Count stride = renderer::Count(sizeof(Packed));
                glNamedBufferData(vbo, renderer::SizePtr(packed.size() * sizeof(Packed)), packed.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
                setupAttrib(1, 3, renderer::Count(3 * sizeof(float)));
                setupAttribI(2, 2, GL_UNSIGNED_INT, renderer::Count(6 * sizeof(float)));
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
                glNamedBufferData(vbo, renderer::SizePtr(interleaved.size() * sizeof(float)), interleaved.data(), GL_STATIC_DRAW);
                glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
                setupAttrib(0, 3, 0);
                setupAttrib(1, 3, renderer::Count(3 * sizeof(float)));
                setupAttrib(2, 2, renderer::Count(6 * sizeof(float)));
            }

            if (indexed) {
                glNamedBufferData(ebo, renderer::SizePtr(index_data.size() * sizeof(GLuint)), index_data.data(), GL_STATIC_DRAW);
                glVertexArrayElementBuffer(vao, ebo);
            }
            const auto primitiveCount = index_data.size() / 3;
            vector<SurfaceId> primitiveSurfaceData = sourcePrimitiveSurfaces;
            if (primitiveSurfaceData.empty()) primitiveSurfaceData.resize(primitiveCount, SurfaceId{0});
            if (primitiveSurfaceData.size() != primitiveCount) {
                glDeleteVertexArrays(1, &vao);
                glDeleteBuffers(1, &vbo);
                if (ebo) glDeleteBuffers(1, &ebo);
                glDeleteBuffers(1, &primitiveSurfaces);
                return context.refuse("resource::geometry::bake: primitive surface count does not match triangle count");
            }
            glNamedBufferData(primitiveSurfaces, renderer::SizePtr(primitiveSurfaceData.size() * sizeof(SurfaceId)), primitiveSurfaceData.data(), GL_STATIC_DRAW);

            umap<primitive::GeometrySemantics::PersistentId, renderer::VertexBuffer> channels;
            if (liveCohesion) {
                renderer::VertexBuffer buffer{0};
                glCreateBuffers(1, &buffer);
                if (not buffer) {
                    glDeleteVertexArrays(1, &vao);
                    glDeleteBuffers(1, &vbo);
                    if (ebo)
                        glDeleteBuffers(1, &ebo);
                    glDeleteBuffers(1, &primitiveSurfaces);
                    return context.refuse("resource::geometry::bake: failed to allocate attrib channel");
                }
                glNamedBufferData(buffer, static_cast<renderer::SizePtr>(cpu.cohesion.size() * sizeof(float)), cpu.cohesion.data(), GL_STATIC_DRAW);
                glEnableVertexArrayAttrib(vao, cohesionLocation);
                glVertexArrayAttribFormat(vao, cohesionLocation, 1, GL_FLOAT, GL_FALSE, 0);
                glVertexArrayAttribBinding(vao, cohesionLocation, 1);
                glVertexArrayVertexBuffer(vao, 1, buffer, 0, renderer::Count(sizeof(float)));
                channels.insert_or_assign(cohesion_id, buffer);
            }

            vec3 boundMin = cpu.positions.front();
            vec3 boundMax = boundMin;
            for (const auto& position : cpu.positions) {
                boundMin = glm::min(boundMin, vec3{position});
                boundMax = glm::max(boundMax, vec3{position});
            }
            return Runtime::Quantum{
                .device = device,
                .vao = vao,
                .vbo = vbo,
                .channels = std::move(channels),
                .ebo = ebo,
                .primitiveSurfaces = primitiveSurfaces,
                .vertex_count = renderer::Count(vertex_count),
                .index_count = renderer::Count(index_data.size()),
                .boundMin = boundMin,
                .boundMax = boundMax,
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
            if (not last.vao && not last.vbo && last.channels.empty() && not last.ebo && not last.primitiveSurfaces) {
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
            for (const auto& channel : last.channels) {
                auto buffer = channel.second;
                if (buffer)
                    glDeleteBuffers(1, &buffer);
            }
            if (last.ebo) {
                auto ebo = last.ebo;
                glDeleteBuffers(1, &ebo);
            }
            if (last.primitiveSurfaces) {
                auto primitiveSurfaces = last.primitiveSurfaces;
                glDeleteBuffers(1, &primitiveSurfaces);
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

        void setSingleEntry(Writing context, Asset::Id assetId, const CpuPresentation& cpu) {
            const auto indexCount = static_cast<renderer::Count>(cpu.indices.empty() ? cpu.positions.size() : cpu.indices.size());
            auto asset = with<Asset>::modify(context, assetId);
            asset->entries = {Asset::Entry{
                .vertices = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(cpu.positions.size())},
                .indices = Asset::Range{.first = renderer::Count{0}, .count = indexCount},
                .surfaces = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{1}},
                .mounts = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}},
                .origin = vec3{0.0f, 0.0f, 0.0f},
            }};
            asset->surfaces = {Asset::Surface{.indices = Asset::Range{.first = renderer::Count{0}, .count = indexCount}}};
            asset->mounts = {};
            asset->entryCatalog = {{"mesh", EntryId{0}}};
            asset->surfaceCatalogs = {{{"surface", SurfaceId{0}}}};
        }

        template<typename Context>
        void writeChannelOn(Context context, Runtime::Id id, primitive::GeometrySemantics::PersistentId semantic, const void* data, renderer::SizePtr bytes) {
            if (semantic == primitive::GeometrySemantics::PersistentId{0} or data == nullptr or bytes == 0 or not with<Runtime>::exists(context, id))
                return;
            const auto& runtime = with<Runtime>::get(context, id);
            const auto found = runtime.channels.find(semantic);
            if (found == runtime.channels.end() or not found->second)
                return;
            if (bytes != static_cast<renderer::SizePtr>(static_cast<std::size_t>(runtime.vertex_count) * sizeof(float)))
                return;
            glfwMakeContextCurrent(with<system::Device>::get(context, runtime.device).handle);
            glNamedBufferSubData(found->second, 0, bytes, data);
        }

    } // namespace

    void Asset::Actions::writeChannel(Writing context, Runtime::Id id, primitive::GeometrySemantics::PersistentId semantic, const void* data, renderer::SizePtr bytes) {
        writeChannelOn(context, id, semantic, data, bytes);
    }

    void Asset::Actions::writeChannel(Stewarding context, Runtime::Id id, primitive::GeometrySemantics::PersistentId semantic, const void* data, renderer::SizePtr bytes) {
        writeChannelOn(context, id, semantic, data, bytes);
    }

    auto Asset::Actions::install(Writing context, Id asset_id, system::Device::Id device, const CpuPresentation& cpu) -> optional<Runtime::Id> {
        setSingleEntry(context, asset_id, cpu);
        auto quantum = bake(context, device, cpu, {});
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
        base::whisper("rmmr: geometry::Loader '{}' ← {}", unit.name.text(), path.string());

        const auto loaded = load_assimp(path);
        if (not loaded) {
            return context.refuse(std::format("resource::geometry::Loader::materialize: Assimp failed '{}'", path.string()));
        }

        {
            auto asset = with<Asset>::modify(context, asset_id);
            asset->entries = loaded->entries;
            asset->surfaces = loaded->surfaces;
            asset->mounts = loaded->mounts;
            asset->entryCatalog = loaded->entryCatalog;
            asset->surfaceCatalogs = loaded->surfaceCatalogs;
        }
        auto quantum = bake(context, device, loaded->cpu, loaded->primitiveSurfaces);
        if (not quantum.vao) return {};
        const auto runtimeId = install_runtime(context, device, asset_id, std::move(quantum));
        with<Runtimes>::modify(context, device)->geometries_id_mapping.insert_or_assign(asset_id, runtimeId);
        return runtimeId;
    }

    auto Generator::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& generator = with<Generator>::get(context, asset_id);
        const auto cpu = cpu_for(generator.type);
        setSingleEntry(context, asset_id, cpu);
        auto quantum = bake(context, device, cpu, {});
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
