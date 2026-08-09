#include <rmmr/system/content/loader_lwo.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

    using rmmr::system::content::LwoDocument;

    // Diagnostic structure only; runtime entry/surface IDs come from geometry::Asset catalogs.
    void log_document(const LwoDocument& doc) {
        std::cout << "file: " << doc.path().string() << '\n';

        std::cout << "materials (" << doc.materials().size() << "):\n";
        for (const auto& material : doc.materials()) {
            std::cout << "  " << material << '\n';
        }

        std::cout << "meshes (" << doc.meshes().size() << "):\n";
        std::uint32_t entryId = 0;
        std::uint32_t surfaceId = 0;
        std::vector<const LwoDocument::Mesh*> meshes;
        for (const auto& mesh : doc.meshes()) meshes.push_back(&mesh);
        std::ranges::sort(meshes, {}, &LwoDocument::Mesh::name);
        for (const auto* meshPointer : meshes) {
            const auto& mesh = *meshPointer;
            std::cout << "  entry " << entryId++ << " '" << mesh.name << "' surfaces=[" << surfaceId << ", " << surfaceId + mesh.submeshes.size() << ")\n";
            std::vector<const LwoDocument::Submesh*> surfaces;
            for (const auto& sub : mesh.submeshes) surfaces.push_back(&sub);
            std::ranges::sort(surfaces, {}, &LwoDocument::Submesh::name);
            for (const auto* subPointer : surfaces) {
                const auto& sub = *subPointer;
                std::cout << "    surface " << surfaceId++ << " '" << sub.name << "'\n";
            }
        }
    }

}

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::cerr << "usage: meshpack_inspector <path.lwo>\n";
        return 2;
    }

    const std::filesystem::path path{argv[1]};
    const auto ext = path.extension().string();
    if (ext != ".lwo" and ext != ".LWO") {
        std::cerr << "meshpack_inspector: unsupported format '" << ext << "' (currently .lwo only)\n";
        return 2;
    }

    auto opened = LwoDocument::open(path);
    if (not opened.document) {
        std::cerr << opened.error << '\n';
        return 1;
    }
    log_document(*opened.document);
    return 0;
}
