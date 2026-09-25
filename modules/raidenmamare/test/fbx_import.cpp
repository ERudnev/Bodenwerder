#include "resources/geometryAssimp.h"

#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>

#include <base/logging.h>
#include <base/testing/macros.h>
#include <base/testing/runner.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

    using rmmr::resource::geometry::assimp::LoadedMesh;

    auto close(float left, float right, float epsilon = 1.0e-4f) -> bool {
        return std::abs(left - right) <= epsilon;
    }

    auto makeMaterial(const char* name) -> aiMaterial* {
        auto* material = new aiMaterial{};
        const aiString value{name};
        material->AddProperty(&value, AI_MATKEY_NAME);
        return material;
    }

    auto makeTriangle(const char* name, unsigned material, std::vector<aiVector3D> positions, std::vector<aiVector3D> normals, std::vector<aiVector3D> uv) -> aiMesh* {
        auto* mesh = new aiMesh{};
        mesh->mName = aiString{name};
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
        mesh->mMaterialIndex = material;
        mesh->mNumVertices = static_cast<unsigned>(positions.size());
        mesh->mVertices = new aiVector3D[mesh->mNumVertices];
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];
        mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
        mesh->mNumUVComponents[0] = 2;
        std::ranges::copy(positions, mesh->mVertices);
        std::ranges::copy(normals, mesh->mNormals);
        std::ranges::copy(uv, mesh->mTextureCoords[0]);
        mesh->mNumFaces = 1;
        mesh->mFaces = new aiFace[1];
        mesh->mFaces[0].mNumIndices = 3;
        mesh->mFaces[0].mIndices = new unsigned[3]{0, 1, 2};
        return mesh;
    }

    auto makeNode(const char* name, std::vector<unsigned> meshes, aiVector3D translation = {}, aiVector3D scale = {1.0f, 1.0f, 1.0f}) -> aiNode* {
        auto* node = new aiNode{name};
        node->mNumMeshes = static_cast<unsigned>(meshes.size());
        node->mMeshes = new unsigned[node->mNumMeshes];
        std::ranges::copy(meshes, node->mMeshes);
        aiMatrix4x4 translationMatrix;
        aiMatrix4x4 scaleMatrix;
        aiMatrix4x4::Translation(translation, translationMatrix);
        aiMatrix4x4::Scaling(scale, scaleMatrix);
        node->mTransformation = translationMatrix * scaleMatrix;
        return node;
    }

    auto makeScene(std::vector<aiMesh*> meshes, std::vector<aiMaterial*> materials, std::vector<aiNode*> children) -> std::unique_ptr<aiScene> {
        auto scene = std::make_unique<aiScene>();
        scene->mNumMeshes = static_cast<unsigned>(meshes.size());
        scene->mMeshes = new aiMesh*[scene->mNumMeshes];
        std::ranges::copy(meshes, scene->mMeshes);
        scene->mNumMaterials = static_cast<unsigned>(materials.size());
        scene->mMaterials = new aiMaterial*[scene->mNumMaterials];
        std::ranges::copy(materials, scene->mMaterials);
        scene->mRootNode = new aiNode{"RootNode"};
        scene->mRootNode->mNumChildren = static_cast<unsigned>(children.size());
        scene->mRootNode->mChildren = new aiNode*[scene->mRootNode->mNumChildren];
        std::ranges::copy(children, scene->mRootNode->mChildren);
        for (auto* child : children) child->mParent = scene->mRootNode;
        return scene;
    }

    auto exportAndLoad(const aiScene& scene, const char* stem) -> LoadedMesh {
        const auto path = std::filesystem::temp_directory_path() / std::format("eltanin_{}.fbx", stem);
        Assimp::Exporter exporter;
        const auto result = exporter.Export(&scene, "fbx", path.string());
        EXPECT_TRUE(result == AI_SUCCESS) << exporter.GetErrorString();
        const auto loaded = rmmr::resource::geometry::assimp::load(path);
        std::filesystem::remove(path);
        EXPECT_TRUE(loaded.has_value()) << "failed to re-import " << path.string();
        return *loaded;
    }

    auto bounds(const LoadedMesh& loaded) -> std::pair<rmmr::vec3, rmmr::vec3> {
        auto low = rmmr::vec3{loaded.cpu.positions.front()};
        auto high = low;
        for (const auto& p : loaded.cpu.positions) {
            low = glm::min(low, rmmr::vec3{p});
            high = glm::max(high, rmmr::vec3{p});
        }
        return {low, high};
    }

    auto flatScene() -> std::unique_ptr<aiScene> {
        auto* mesh = makeTriangle(
            "flat_mesh", 0,
            {{0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, {0.0f, 100.0f, 0.0f}},
            {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
        return makeScene({mesh}, {makeMaterial("tile_surface")}, {makeNode("tile_flat", {0}, {100.0f, 200.0f, 300.0f})});
    }

    auto wedgeScene() -> std::unique_ptr<aiScene> {
        const auto normal = aiVector3D{0.0f, -0.70710678f, 0.70710678f};
        auto* mesh = makeTriangle(
            "wedge_mesh", 0,
            {{0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, {0.0f, 100.0f, 100.0f}},
            {normal, normal, normal},
            {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}});
        // A reflected node is representative of the p222A/p222V Max exports.
        // Baking it must also reverse triangle order so normals remain CCW.
        return makeScene({mesh}, {makeMaterial("wedge_surface")}, {makeNode("tile_wedge", {0}, {}, {-1.0f, 1.0f, 1.0f})});
    }

    auto multipartScene() -> std::unique_ptr<aiScene> {
        const std::vector<aiVector3D> positions{{0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, {0.0f, 100.0f, 0.0f}};
        const std::vector<aiVector3D> normals(3, aiVector3D{0.0f, 0.0f, 1.0f});
        const std::vector<aiVector3D> uv{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        return makeScene(
            {
                makeTriangle("body_mesh", 0, positions, normals, uv),
                makeTriangle("trim_mesh", 1, positions, normals, uv),
                makeTriangle("glass_mesh", 2, positions, normals, uv),
            },
            {makeMaterial("body"), makeMaterial("trim"), makeMaterial("glass")},
            {makeNode("tile_multi", {0, 1, 2})});
    }

} // namespace

namespace tests {

    void existing_lwo_contract_is_unchanged() {
        const auto path = std::filesystem::path{DAQL_ASSETS_DIR} / "Eltanin/meshes/editor/interframe.lwo";
        const auto loaded = rmmr::resource::geometry::assimp::load(path);
        EXPECT_TRUE(loaded.has_value());
        EXPECT_TRUE(not loaded->entryCatalog.empty());
        EXPECT_TRUE(loaded->surfaceCatalogs.size() == loaded->entries.size());
    }

    void fbx_flat_tile() {
        const auto loaded = exportAndLoad(*flatScene(), "flat_tile");
        EXPECT_TRUE(loaded.entryCatalog.contains("tile_flat"));
        EXPECT_TRUE(loaded.surfaceCatalogs.at(loaded.entryCatalog.at("tile_flat")).contains("tile_surface"));
        const auto [low, high] = bounds(loaded);
        const auto size = high - low;
        EXPECT_TRUE(close(glm::length(size), std::sqrt(5.0f))) << "unexpected metre scale: " << size.x << ", " << size.y << ", " << size.z;
        const auto origin = loaded.entries.at(loaded.entryCatalog.at("tile_flat")).origin;
        EXPECT_TRUE(close(glm::length(origin), 0.0f)) << "FBX tile entry must use the assembler origin: " << origin.x << ", " << origin.y << ", " << origin.z;
        EXPECT_TRUE(close(low.x, 1.0f) and close(low.y, 2.0f) and close(low.z, 3.0f))
            << "node transform was not baked into authored tile space: " << low.x << ", " << low.y << ", " << low.z;
        EXPECT_TRUE(loaded.cpu.uv0.size() == loaded.cpu.positions.size());
    }

    void fbx_wedge() {
        const auto loaded = exportAndLoad(*wedgeScene(), "wedge");
        EXPECT_TRUE(loaded.entryCatalog.contains("tile_wedge"));
        EXPECT_TRUE(loaded.cpu.indices.size() == 3);
        for (const auto& normal : loaded.cpu.normals) {
            EXPECT_TRUE(close(glm::length(rmmr::vec3{normal}), 1.0f));
        }
        const auto i0 = loaded.cpu.indices[0];
        const auto i1 = loaded.cpu.indices[1];
        const auto i2 = loaded.cpu.indices[2];
        const auto faceNormal = glm::normalize(glm::cross(rmmr::vec3{loaded.cpu.positions[i1]} - rmmr::vec3{loaded.cpu.positions[i0]}, rmmr::vec3{loaded.cpu.positions[i2]} - rmmr::vec3{loaded.cpu.positions[i0]}));
        EXPECT_TRUE(glm::dot(faceNormal, rmmr::vec3{loaded.cpu.normals[i0]}) > 0.99f) << "winding and normals disagree";
    }

    void fbx_named_parts_and_material_slots() {
        const auto loaded = exportAndLoad(*multipartScene(), "multi");
        EXPECT_TRUE(loaded.entryCatalog.size() == 1);
        EXPECT_TRUE(loaded.entryCatalog.contains("tile_multi"));
        const auto entry = loaded.entryCatalog.at("tile_multi");
        const auto& surfaces = loaded.surfaceCatalogs.at(entry);
        EXPECT_TRUE(surfaces.size() == 3);
        EXPECT_TRUE(surfaces.contains("body"));
        EXPECT_TRUE(surfaces.contains("trim"));
        EXPECT_TRUE(surfaces.contains("glass"));
    }

} // namespace tests

int main(int argc, char** argv) {
    if (argc > 1) {
        for (int argument = 1; argument < argc; ++argument) {
            const auto path = std::filesystem::path{argv[argument]};
            const auto loaded = rmmr::resource::geometry::assimp::load(path);
            if (not loaded) {
                std::cerr << "failed to load " << path.string() << '\n';
                return 2;
            }
            std::cout << "FILE " << path.filename().string() << '\n';
            Assimp::Importer sourceImporter;
            if (const auto* source = sourceImporter.ReadFile(path.string(), 0)) {
                for (unsigned materialIndex = 0; materialIndex < source->mNumMaterials; ++materialIndex) {
                    aiString materialName;
                    source->mMaterials[materialIndex]->Get(AI_MATKEY_NAME, materialName);
                    std::cout << "  MATERIAL " << materialName.C_Str();
                    aiString texture;
                    if (source->mMaterials[materialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS) {
                        std::cout << " diffuse=" << texture.C_Str();
                    }
                    std::cout << '\n';
                }
            }
            float minimumNormalLength = std::numeric_limits<float>::max();
            float maximumNormalLength = 0.0f;
            std::size_t invalidNormals = 0;
            for (const auto& packed : loaded->cpu.normals) {
                const auto normal = rmmr::vec3{packed};
                const auto length = glm::length(normal);
                if (not std::isfinite(length)) ++invalidNormals;
                minimumNormalLength = std::min(minimumNormalLength, length);
                maximumNormalLength = std::max(maximumNormalLength, length);
            }
            std::size_t invalidUv = 0;
            for (const auto& uv : loaded->cpu.uv0) {
                if (not std::isfinite(uv.x) or not std::isfinite(uv.y)) ++invalidUv;
            }
            std::size_t reversedTriangles = 0;
            std::size_t degenerateTriangles = 0;
            for (std::size_t index = 0; index + 2 < loaded->cpu.indices.size(); index += 3) {
                const auto i0 = loaded->cpu.indices[index];
                const auto i1 = loaded->cpu.indices[index + 1];
                const auto i2 = loaded->cpu.indices[index + 2];
                const auto p0 = rmmr::vec3{loaded->cpu.positions.at(i0)};
                const auto p1 = rmmr::vec3{loaded->cpu.positions.at(i1)};
                const auto p2 = rmmr::vec3{loaded->cpu.positions.at(i2)};
                const auto cross = glm::cross(p1 - p0, p2 - p0);
                if (glm::dot(cross, cross) <= 1.0e-12f) {
                    ++degenerateTriangles;
                    continue;
                }
                const auto normal = rmmr::vec3{loaded->cpu.normals.at(i0)}
                    + rmmr::vec3{loaded->cpu.normals.at(i1)}
                    + rmmr::vec3{loaded->cpu.normals.at(i2)};
                if (glm::dot(normal, normal) > 1.0e-12f and glm::dot(cross, normal) < 0.0f) ++reversedTriangles;
            }
            std::cout << "  VALIDATION vertices=" << loaded->cpu.positions.size()
                      << " triangles=" << loaded->cpu.indices.size() / 3
                      << " uv0=" << loaded->cpu.uv0.size()
                      << " invalid_uv=" << invalidUv
                      << " normals=" << loaded->cpu.normals.size()
                      << " invalid_normals=" << invalidNormals
                      << " normal_length=" << minimumNormalLength << ".." << maximumNormalLength
                      << " reversed_triangles=" << reversedTriangles
                      << " degenerate_triangles=" << degenerateTriangles << '\n';
            std::vector<std::pair<std::string, rmmr::resource::geometry::EntryId>> entries;
            for (const auto& named : loaded->entryCatalog) entries.push_back(named);
            std::ranges::sort(entries, {}, &std::pair<std::string, rmmr::resource::geometry::EntryId>::first);
            for (const auto& [name, id] : entries) {
                const auto& entry = loaded->entries.at(static_cast<std::size_t>(id));
                auto low = rmmr::vec3{loaded->cpu.positions.at(entry.vertices.first)};
                auto high = low;
                for (auto vertex = entry.vertices.first; vertex < entry.vertices.first + entry.vertices.count; ++vertex) {
                    const auto position = rmmr::vec3{loaded->cpu.positions.at(vertex)};
                    low = glm::min(low, position);
                    high = glm::max(high, position);
                }
                std::cout << "  ENTRY " << name
                          << " origin=" << entry.origin.x << ',' << entry.origin.y << ',' << entry.origin.z
                          << " min=" << low.x << ',' << low.y << ',' << low.z
                          << " max=" << high.x << ',' << high.y << ',' << high.z
                          << " size=" << high.x - low.x << ',' << high.y - low.y << ',' << high.z - low.z
                          << " vertices=" << entry.vertices.count << " indices=" << entry.indices.count << '\n';
                std::vector<std::string> surfaces;
                for (const auto& [surface, _] : loaded->surfaceCatalogs.at(static_cast<std::size_t>(id))) surfaces.push_back(surface);
                std::ranges::sort(surfaces);
                for (const auto& surface : surfaces) std::cout << "    SURFACE " << surface << '\n';
            }
        }
        return 0;
    }
    const auto summary = base::testing::run_tests(BASETEST_LIST(
        BASETEST_NAMED("existing LWO loader contract", &tests::existing_lwo_contract_is_unchanged),
        BASETEST_NAMED("fbx flat tile: metres/pivot/normals/uv", &tests::fbx_flat_tile),
        BASETEST_NAMED("fbx wedge: orientation/winding", &tests::fbx_wedge),
        BASETEST_NAMED("fbx module: object/material names", &tests::fbx_named_parts_and_material_slots)));
    return summary.ok() ? 0 : 1;
}
