#include <rmmr/system/content/loader_lwo.h>

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <format>
#include <string_view>

namespace rmmr::system::content {

    namespace {

        auto material_name_of(const aiScene& scene, unsigned material_index) -> std::string {
            if (material_index >= scene.mNumMaterials) {
                return {};
            }
            aiString name;
            if (scene.mMaterials[material_index]->Get(AI_MATKEY_NAME, name) != AI_SUCCESS or name.length == 0) {
                return {};
            }
            return name.C_Str();
        }

        auto is_synthetic_node(std::string_view name) -> bool {
            return name.empty() or name == "<LWORoot>" or name.starts_with("Pivot-");
        }

        // Drop LightWave layer suffix from first '=' (e.g. "…=default").
        auto mesh_identity(std::string_view raw) -> std::string {
            const auto eq = raw.find('=');
            if (eq == std::string_view::npos or eq == 0) {
                return std::string(raw);
            }
            return std::string(raw.substr(0, eq));
        }

        auto uniquify(std::string base, const std::vector<std::string>& taken) -> std::string {
            auto unique = base;
            for (std::size_t n = taken.size(); std::find(taken.begin(), taken.end(), unique) != taken.end(); ++n) {
                unique = std::format("{}_{}", base, n);
            }
            return unique;
        }

        void collect_meshes(const aiScene& scene, const aiNode& node, std::vector<LwoDocument::Mesh>& out) {
            if (node.mNumMeshes > 0 and not is_synthetic_node(node.mName.C_Str())) {
                LwoDocument::Mesh mesh;
                mesh.name = mesh_identity(node.mName.C_Str());
                std::vector<std::string> taken;
                for (unsigned i = 0; i < node.mNumMeshes; ++i) {
                    const auto* ai_mesh = scene.mMeshes[node.mMeshes[i]];
                    if (not ai_mesh or ai_mesh->mNumVertices == 0) {
                        continue;
                    }
                    auto material = material_name_of(scene, ai_mesh->mMaterialIndex);
                    if (material.empty()) {
                        material = std::format("material_{}", taken.size());
                    }
                    const auto name = uniquify(std::move(material), taken);
                    taken.push_back(name);
                    mesh.submeshes.push_back(LwoDocument::Submesh{.name = name});
                }
                if (not mesh.submeshes.empty()) {
                    out.push_back(std::move(mesh));
                }
            }
            for (unsigned i = 0; i < node.mNumChildren; ++i) {
                collect_meshes(scene, *node.mChildren[i], out);
            }
        }

    } // namespace

    struct LwoDocument::Impl {
        Assimp::Importer importer;
        const aiScene* scene;
        std::filesystem::path path;
        std::vector<std::string> materials;
        std::vector<Mesh> meshes;
    };

    LwoDocument::LwoDocument(std::unique_ptr<Impl> impl)
        : impl_(std::move(impl))
    {
    }

    LwoDocument::LwoDocument(LwoDocument&&) noexcept = default;
    auto LwoDocument::operator=(LwoDocument&&) noexcept -> LwoDocument& = default;
    LwoDocument::~LwoDocument() = default;

    auto LwoDocument::open(const std::filesystem::path& path) -> OpenResult {
        auto impl = std::make_unique<Impl>();
        impl->path = path;
        impl->scene = impl->importer.ReadFile(
            path.string(),
            aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_FlipUVs);
        if (not impl->scene or not impl->scene->mRootNode or (impl->scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) or impl->scene->mNumMeshes == 0) {
            return OpenResult{
                .document = {},
                .error = std::format("Assimp failed '{}': {}", path.string(), impl->importer.GetErrorString()),
            };
        }

        bool missing_normals = false;
        for (unsigned i = 0; i < impl->scene->mNumMeshes; ++i) {
            const auto* mesh = impl->scene->mMeshes[i];
            if (mesh and mesh->mNumVertices > 0 and not mesh->HasNormals()) {
                missing_normals = true;
                break;
            }
        }
        if (missing_normals) {
            impl->scene = impl->importer.ApplyPostProcessing(aiProcess_GenSmoothNormals);
            if (not impl->scene) {
                return OpenResult{.document = {}, .error = std::format("Assimp GenSmoothNormals failed '{}'", path.string())};
            }
        }
        impl->scene = impl->importer.ApplyPostProcessing(aiProcess_JoinIdenticalVertices);
        if (not impl->scene) {
            return OpenResult{.document = {}, .error = std::format("Assimp JoinIdenticalVertices failed '{}'", path.string())};
        }

        impl->materials.reserve(impl->scene->mNumMaterials);
        for (unsigned i = 0; i < impl->scene->mNumMaterials; ++i) {
            auto name = material_name_of(*impl->scene, i);
            if (name.empty()) {
                name = std::format("(unnamed_{})", i);
            }
            impl->materials.push_back(std::move(name));
        }

        collect_meshes(*impl->scene, *impl->scene->mRootNode, impl->meshes);

        return OpenResult{
            .document = std::unique_ptr<LwoDocument>(new LwoDocument(std::move(impl))),
            .error = {},
        };
    }

    auto LwoDocument::path() const -> const std::filesystem::path& {
        return impl_->path;
    }

    auto LwoDocument::materials() const -> const std::vector<std::string>& {
        return impl_->materials;
    }

    auto LwoDocument::meshes() const -> const std::vector<Mesh>& {
        return impl_->meshes;
    }

}
