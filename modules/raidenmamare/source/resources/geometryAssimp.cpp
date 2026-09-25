#include "geometryAssimp.h"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <string>
#include <vector>

namespace rmmr::resource::geometry::assimp {

    namespace {

        struct EntryNode {
            const aiNode* node;
            mat4 world;
        };

        auto lowerExtension(const filepath& path) -> string {
            auto extension = path.extension().string();
            std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension;
        }

        auto fromAssimp(const aiMatrix4x4& m) -> mat4 {
            return mat4{
                m.a1, m.b1, m.c1, m.d1,
                m.a2, m.b2, m.c2, m.d2,
                m.a3, m.b3, m.c3, m.d3,
                m.a4, m.b4, m.c4, m.d4};
        }

        auto materialName(const aiScene& scene, const aiMesh& mesh) -> string {
            if (mesh.mMaterialIndex >= scene.mNumMaterials) return {};
            aiString name;
            if (scene.mMaterials[mesh.mMaterialIndex]->Get(AI_MATKEY_NAME, name) != AI_SUCCESS or name.length == 0) return {};
            return name.C_Str();
        }

        void appendMesh(LoadedMesh& out, const aiMesh& mesh, const mat4& transform, umap<string, SurfaceId>& catalog, const aiScene& scene) {
            const auto base = static_cast<integer>(out.cpu.positions.size());
            const auto normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
            const bool mirrored = glm::determinant(glm::mat3(transform)) < 0.0f;
            out.cpu.positions.reserve(out.cpu.positions.size() + mesh.mNumVertices);
            out.cpu.normals.reserve(out.cpu.normals.size() + mesh.mNumVertices);
            out.cpu.uv0.reserve(out.cpu.uv0.size() + mesh.mNumVertices);
            for (unsigned i = 0; i < mesh.mNumVertices; ++i) {
                const auto& p = mesh.mVertices[i];
                const auto baked = transform * vec4{p.x, p.y, p.z, 1.0f};
                out.cpu.positions.push_back(Pos{baked.x, baked.y, baked.z});
                if (mesh.HasNormals()) {
                    const auto& n = mesh.mNormals[i];
                    auto normal = normalMatrix * vec3{n.x, n.y, n.z};
                    if (glm::dot(normal, normal) > 0.0f) normal = glm::normalize(normal);
                    out.cpu.normals.push_back(Pos{normal.x, normal.y, normal.z});
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
                if (face.mNumIndices != 3) continue;
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[0]));
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[mirrored ? 2 : 1]));
                out.cpu.indices.push_back(base + static_cast<integer>(face.mIndices[mirrored ? 1 : 2]));
                out.primitiveSurfaces.push_back(surface);
            }
            const auto indexCount = static_cast<renderer::Count>(out.cpu.indices.size()) - indexStart;
            out.surfaces.push_back(Asset::Surface{.indices = Asset::Range{.first = indexStart, .count = indexCount}});
            auto surfaceName = materialName(scene, mesh);
            if (surfaceName.empty()) surfaceName = std::format("surface_{}", catalog.size());
            if (catalog.contains(surfaceName)) surfaceName = std::format("{}_{}", surfaceName, catalog.size());
            catalog.emplace(std::move(surfaceName), surface);
        }

        void collectEntryNodes(const aiNode& node, vector<const aiNode*>& nodes) {
            if (node.mNumMeshes > 0) nodes.push_back(&node);
            for (unsigned i = 0; i < node.mNumChildren; ++i) collectEntryNodes(*node.mChildren[i], nodes);
        }

        void collectEntryNodes(const aiNode& node, const mat4& parent, vector<EntryNode>& nodes) {
            const auto world = parent * fromAssimp(node.mTransformation);
            if (node.mNumMeshes > 0) nodes.push_back(EntryNode{.node = &node, .world = world});
            for (unsigned i = 0; i < node.mNumChildren; ++i) collectEntryNodes(*node.mChildren[i], world, nodes);
        }

        auto aiTranslation(const aiMatrix4x4& m) -> vec3 { return vec3{m.a4, m.b4, m.c4}; }

        auto lwoOrigin(const aiNode& node) -> vec3 {
            if (node.mParent) {
                const string parentName{node.mParent->mName.C_Str()};
                if (parentName.starts_with("Pivot-")) return aiTranslation(node.mParent->mTransformation);
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

        void appendEntry(LoadedMesh& out, const aiScene& scene, const aiNode& node, const mat4& transform, const vec3& origin) {
            const auto firstVertex = static_cast<renderer::Count>(out.cpu.positions.size());
            const auto firstIndex = static_cast<renderer::Count>(out.cpu.indices.size());
            const auto firstSurface = static_cast<renderer::Count>(out.surfaces.size());
            umap<string, SurfaceId> catalog;
            vector<const aiMesh*> meshes;
            for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                const auto* mesh = scene.mMeshes[node.mMeshes[i]];
                if (mesh and mesh->mNumVertices > 0) meshes.push_back(mesh);
            }
            std::ranges::sort(meshes, [&](const aiMesh* left, const aiMesh* right) {
                const auto leftMaterial = materialName(scene, *left);
                const auto rightMaterial = materialName(scene, *right);
                if (leftMaterial != rightMaterial) return leftMaterial < rightMaterial;
                return string{left->mName.C_Str()} < string{right->mName.C_Str()};
            });
            for (const auto* mesh : meshes) appendMesh(out, *mesh, transform, catalog, scene);
            if (static_cast<renderer::Count>(out.cpu.indices.size()) == firstIndex) return;

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

        void appendLwoEntry(LoadedMesh& out, const aiScene& scene, const aiNode& node) {
            const auto firstVertex = static_cast<renderer::Count>(out.cpu.positions.size());
            const auto origin = lwoOrigin(node);
            appendEntry(out, scene, node, mat4{1.0f}, origin);
            bakeOriginIntoVertices(out, firstVertex, origin);
        }

        void appendFbxEntry(LoadedMesh& out, const aiScene& scene, const EntryNode& entry) {
            // 3ds Max FBX exports mesh vertices in object/file space and keeps the
            // object's pivot as a compensating node transform. Bake that transform
            // to recover the authored tile coordinates. The assembler anchors FBX
            // entries at the tile origin, exactly like the existing LWO tiles.
            appendEntry(out, scene, *entry.node, entry.world, vec3{0.0f});
        }

        void appendWhole(LoadedMesh& out, const aiScene& scene, string name) {
            const auto firstIndex = static_cast<renderer::Count>(out.cpu.indices.size());
            umap<string, SurfaceId> catalog;
            vector<const aiMesh*> meshes;
            for (unsigned index = 0; index < scene.mNumMeshes; ++index) {
                const auto* mesh = scene.mMeshes[index];
                if (mesh and mesh->mNumVertices > 0) meshes.push_back(mesh);
            }
            std::ranges::sort(meshes, [&](const aiMesh* left, const aiMesh* right) { return materialName(scene, *left) < materialName(scene, *right); });
            for (const auto* mesh : meshes) appendMesh(out, *mesh, mat4{1.0f}, catalog, scene);
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

        auto readScene(Assimp::Importer& importer, const filepath& path, bool fbx) -> const aiScene* {
            unsigned flags = aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_FlipUVs;
            if (fbx) {
                importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
                flags |= aiProcess_GlobalScale;
            }
            const auto* scene = importer.ReadFile(path.string(), flags);
            if (not scene or not scene->mRootNode or (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) or scene->mNumMeshes == 0) return nullptr;

            bool missingNormals = false;
            for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
                const auto* mesh = scene->mMeshes[meshIndex];
                if (mesh and mesh->mNumVertices > 0 and not mesh->HasNormals()) {
                    missingNormals = true;
                    break;
                }
            }
            if (missingNormals) {
                scene = importer.ApplyPostProcessing(aiProcess_GenSmoothNormals);
                if (not scene) return nullptr;
            }
            return importer.ApplyPostProcessing(aiProcess_JoinIdenticalVertices);
        }

    }

    auto load(const filepath& path) -> optional<LoadedMesh> {
        Assimp::Importer importer;
        const auto extension = lowerExtension(path);
        const bool lwo = extension == ".lwo";
        const bool fbx = extension == ".fbx";
        const auto* scene = readScene(importer, path, fbx);
        if (not scene) return {};

        LoadedMesh out{};
        out.cpu.layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"});

        if (lwo) {
            vector<const aiNode*> nodes;
            collectEntryNodes(*scene->mRootNode, nodes);
            std::ranges::sort(nodes, [](const aiNode* left, const aiNode* right) { return string{left->mName.C_Str()} < string{right->mName.C_Str()}; });
            for (const auto* node : nodes) appendLwoEntry(out, *scene, *node);
        } else if (fbx) {
            vector<EntryNode> nodes;
            collectEntryNodes(*scene->mRootNode, mat4{1.0f}, nodes);
            std::ranges::sort(nodes, [](const EntryNode& left, const EntryNode& right) { return string{left.node->mName.C_Str()} < string{right.node->mName.C_Str()}; });
            for (const auto& node : nodes) appendFbxEntry(out, *scene, node);
        } else {
            appendWhole(out, *scene, path.stem().string());
        }

        if (out.cpu.positions.empty() or out.cpu.indices.empty()) return {};
        return out;
    }

}
