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
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
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
            // RH OpenGL: do not pass MakeLeftHanded / FlipWindingOrder here (would double LWO: importer already does both in GenerateNodeGraph).
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

            return out;
        }

        struct CpuChannel {
            const std::byte* data;
            std::size_t count;
            std::size_t stride;
        };

        auto cpuChannel(const CpuPresentation& cpu, primitive::GeometrySemantics::PersistentId id) -> CpuChannel {
            const auto view = []<typename Value>(const vector<Value>& values) {
                return CpuChannel{reinterpret_cast<const std::byte*>(values.data()), values.size(), sizeof(Value)};
            };
            using Semantics = primitive::GeometrySemantics;
            if (id == Semantics::id_of("position")) return view(cpu.positions);
            if (id == Semantics::id_of("normal")) return view(cpu.normals);
            if (id == Semantics::id_of("uv0")) return view(cpu.uv0);
            if (id == Semantics::id_of("color0")) return view(cpu.color0);
            if (id == Semantics::id_of("mix0")) return view(cpu.mix0);
            if (id == Semantics::id_of("cohesion")) return view(cpu.cohesion);
            if (id == Semantics::id_of("palette")) return view(cpu.palette);
            if (id == Semantics::id_of("weights")) return view(cpu.weights);
            return CpuChannel{nullptr, 0, 0};
        }

        auto bake(Writing context, system::Device::Id device, const CpuPresentation& cpu, const vector<SurfaceId>& sourcePrimitiveSurfaces) -> Runtime::Quantum {
            using Semantics = primitive::GeometrySemantics;
            if (cpu.positions.empty()) return context.refuse("resource::geometry::bake: positions are empty");
            if (cpu.layout.empty() or cpu.layout.front() != Semantics::id_of("position")) return context.refuse("resource::geometry::bake: layout must start with position");

            const auto vertexCount = cpu.positions.size();
            struct Attrib {
                GLuint location;
                Semantics::PersistentId id;
                Semantics::Type type;
                bool live;
                const std::byte* data;
                std::size_t stride;
            };
            vector<Attrib> attribs;
            attribs.reserve(cpu.layout.size());
            for (std::size_t index = 0; index < cpu.layout.size(); ++index) {
                const auto id = cpu.layout[index];
                const auto* entry = Semantics::find(id);
                if (entry == nullptr or entry->id == Semantics::PersistentId{0}) return context.refuse("resource::geometry::bake: unknown vertex attribute");
                for (const auto& attrib : attribs) {
                    if (attrib.id == id) return context.refuse(std::format("resource::geometry::bake: duplicate vertex attribute '{}'", entry->name));
                }
                const auto channel = cpuChannel(cpu, id);
                if (channel.count != vertexCount) return context.refuse(std::format("resource::geometry::bake: {} count must match positions", entry->name));
                if (channel.stride != Semantics::byteSize(entry->type)) return context.refuse(std::format("resource::geometry::bake: {} storage does not match semantic type", entry->name));
                attribs.push_back(Attrib{.location = static_cast<GLuint>(index), .id = id, .type = entry->type, .live = entry->live, .data = channel.data, .stride = channel.stride});
            }
            for (const auto& entry : Semantics::vocabulary) {
                if (entry.id == Semantics::PersistentId{0}) continue;
                bool used = false;
                for (const auto& attrib : attribs) {
                    if (attrib.id == entry.id) {
                        used = true;
                        break;
                    }
                }
                if (used) continue;
                if (cpuChannel(cpu, entry.id).count != 0) return context.refuse(std::format("resource::geometry::bake: {} must be empty for this layout", entry.name));
            }

            vector<GLuint> indexData;
            if (not cpu.indices.empty()) {
                indexData.reserve(cpu.indices.size());
                for (const auto index : cpu.indices) {
                    if (index < 0 or static_cast<std::size_t>(index) >= vertexCount) return context.refuse("resource::geometry::bake: index out of positions range");
                    indexData.push_back(static_cast<GLuint>(index));
                }
            } else {
                indexData.reserve(vertexCount);
                for (std::size_t index = 0; index < vertexCount; ++index) indexData.push_back(static_cast<GLuint>(index));
            }

            const auto primitiveCount = indexData.size() / 3;
            vector<SurfaceId> primitiveSurfaceData = sourcePrimitiveSurfaces;
            if (primitiveSurfaceData.empty()) primitiveSurfaceData.resize(primitiveCount, SurfaceId{0});
            if (primitiveSurfaceData.size() != primitiveCount) return context.refuse("resource::geometry::bake: primitive surface count does not match triangle count");

            std::size_t packedStride = 0;
            for (const auto& attrib : attribs) {
                if (not attrib.live) packedStride += attrib.stride;
            }
            vector<std::byte> packed(vertexCount * packedStride);
            for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
                auto* dest = packed.data() + vertex * packedStride;
                std::size_t offset = 0;
                for (const auto& attrib : attribs) {
                    if (attrib.live) continue;
                    std::memcpy(dest + offset, attrib.data + vertex * attrib.stride, attrib.stride);
                    offset += attrib.stride;
                }
            }

            glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);
            renderer::VertexArray vao{};
            renderer::VertexBuffer vbo{};
            renderer::ElementBuffer ebo{};
            renderer::StorageBuffer primitiveSurfaces{};
            umap<Semantics::PersistentId, renderer::VertexBuffer> channels;
            auto release = [&] {
                if (vao) glDeleteVertexArrays(1, &vao);
                if (vbo) glDeleteBuffers(1, &vbo);
                if (ebo) glDeleteBuffers(1, &ebo);
                if (primitiveSurfaces) glDeleteBuffers(1, &primitiveSurfaces);
                for (auto& pair : channels) {
                    if (pair.second) glDeleteBuffers(1, &pair.second);
                }
            };
            glCreateVertexArrays(1, &vao);
            glCreateBuffers(1, &vbo);
            glCreateBuffers(1, &ebo);
            glCreateBuffers(1, &primitiveSurfaces);
            if (not vao or not vbo or not ebo or not primitiveSurfaces) {
                release();
                return context.refuse("resource::geometry::bake: failed to allocate VAO/VBO/EBO/primitive-surface SSBO");
            }

            auto bindAttrib = [&](const Attrib& attrib, GLuint binding, GLuint relativeOffset) {
                glEnableVertexArrayAttrib(vao, attrib.location);
                const auto components = static_cast<GLint>(Semantics::componentCount(attrib.type));
                if (Semantics::integerPacked(attrib.type)) glVertexArrayAttribIFormat(vao, attrib.location, components, GL_UNSIGNED_INT, relativeOffset);
                else glVertexArrayAttribFormat(vao, attrib.location, components, GL_FLOAT, GL_FALSE, relativeOffset);
                glVertexArrayAttribBinding(vao, attrib.location, binding);
            };

            glNamedBufferData(vbo, renderer::SizePtr(packed.size()), packed.data(), GL_STATIC_DRAW);
            glVertexArrayVertexBuffer(vao, 0, vbo, 0, renderer::Count(packedStride));
            std::size_t packedOffset = 0;
            for (const auto& attrib : attribs) {
                if (attrib.live) continue;
                bindAttrib(attrib, 0, static_cast<GLuint>(packedOffset));
                packedOffset += attrib.stride;
            }
            glNamedBufferData(ebo, renderer::SizePtr(indexData.size() * sizeof(GLuint)), indexData.data(), GL_STATIC_DRAW);
            glVertexArrayElementBuffer(vao, ebo);
            glNamedBufferData(primitiveSurfaces, renderer::SizePtr(primitiveSurfaceData.size() * sizeof(SurfaceId)), primitiveSurfaceData.data(), GL_STATIC_DRAW);

            GLuint liveBinding = 1;
            for (const auto& attrib : attribs) {
                if (not attrib.live) continue;
                renderer::VertexBuffer buffer{0};
                glCreateBuffers(1, &buffer);
                if (not buffer) {
                    release();
                    return context.refuse("resource::geometry::bake: failed to allocate attrib channel");
                }
                glNamedBufferData(buffer, renderer::SizePtr(vertexCount * attrib.stride), attrib.data, GL_STATIC_DRAW);
                bindAttrib(attrib, liveBinding, 0);
                glVertexArrayVertexBuffer(vao, liveBinding, buffer, 0, renderer::Count(attrib.stride));
                channels.insert_or_assign(attrib.id, buffer);
                liveBinding += 1;
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
                .vertex_count = renderer::Count(vertexCount),
                .index_count = renderer::Count(indexData.size()),
                .boundMin = boundMin,
                .boundMax = boundMax,
            };
        }

        auto cpu_for(const Generator::Quantum& generator) -> CpuPresentation {
            switch (generator.type) {
                case Generator::Type::triangle: return GeometryGenerator::triangle();
                case Generator::Type::kube: return GeometryGenerator::kube();
                case Generator::Type::bagel: return GeometryGenerator::bagel();
                case Generator::Type::gridPlane: return GeometryGenerator::gridPlane();
                case Generator::Type::unitQuad: return GeometryGenerator::unitQuad();
                case Generator::Type::sphere: return GeometryGenerator::sphere(generator.subdivisions);
                case Generator::Type::diamond: return GeometryGenerator::diamond();
                case Generator::Type::patchGrid: return GeometryGenerator::patchGrid(generator.subdivisions);
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

        void setCataloguedEntry(Writing context, Asset::Id assetId, const CpuPresentation& cpu, const vector<SurfaceId>& primitiveSurfaces, const umap<string, SurfaceId>& catalog) {
            const auto indexCount = static_cast<renderer::Count>(cpu.indices.empty() ? cpu.positions.size() : cpu.indices.size());
            SurfaceId surfaceCount{0};
            for (const auto surface : primitiveSurfaces)
                if (surface >= surfaceCount) surfaceCount = surface + 1;
            vector<Asset::Surface> surfaces(static_cast<std::size_t>(surfaceCount), Asset::Surface{.indices = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}}});
            for (std::size_t triangle = 0; triangle < primitiveSurfaces.size(); ++triangle) {
                auto& range = surfaces[static_cast<std::size_t>(primitiveSurfaces[triangle])].indices;
                const auto first = static_cast<renderer::Count>(triangle * 3);
                if (range.count == renderer::Count{0})
                    range.first = first;
                range.count += renderer::Count{3};
            }
            auto asset = with<Asset>::modify(context, assetId);
            asset->entries = {Asset::Entry{
                .vertices = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(cpu.positions.size())},
                .indices = Asset::Range{.first = renderer::Count{0}, .count = indexCount},
                .surfaces = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(surfaceCount)},
                .mounts = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}},
                .origin = vec3{0.0f, 0.0f, 0.0f},
            }};
            asset->surfaces = std::move(surfaces);
            asset->mounts = {};
            asset->entryCatalog = {{"mesh", EntryId{0}}};
            asset->surfaceCatalogs = {catalog};
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

    auto Asset::Actions::install(Writing context, Id asset_id, system::Device::Id device, const CpuPresentation& cpu, const vector<SurfaceId>& primitiveSurfaces, const umap<string, SurfaceId>& surfaceCatalog) -> optional<Runtime::Id> {
        const auto indexCount = cpu.indices.empty() ? cpu.positions.size() : cpu.indices.size();
        if (cpu.positions.empty() or indexCount % 3 != 0)
            return context.refuse("resource::geometry::Asset::install: mesh is empty or not triangulated");
        if (primitiveSurfaces.size() != indexCount / 3)
            return context.refuse("resource::geometry::Asset::install: primitive surface count does not match triangle count");
        if (surfaceCatalog.empty())
            return context.refuse("resource::geometry::Asset::install: surface catalog is empty");
        setCataloguedEntry(context, asset_id, cpu, primitiveSurfaces, surfaceCatalog);
        auto quantum = bake(context, device, cpu, primitiveSurfaces);
        if (not quantum.vao)
            return {};
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
        const auto cpu = cpu_for(generator);
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

    auto doctrine::geometry() -> Schema {
        return ask::schema::merge({
            ask::schema::aspect<Runtime>(),
            ask::schema::aspect<Asset>(),
            ask::schema::aspect<Loader>(),
            ask::schema::aspect<Generator>(),
        });
    }

}
