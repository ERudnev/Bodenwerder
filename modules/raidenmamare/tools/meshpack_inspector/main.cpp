#include <rmmr/system/content/loader_lwo.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

    using rmmr::system::content::LwoDocument;

    void log_document(const LwoDocument& doc) {
        std::cout << "file: " << doc.path().string() << '\n';

        std::cout << "materials (" << doc.materials().size() << "):\n";
        for (const auto& material : doc.materials()) {
            std::cout << "  " << material << '\n';
        }

        std::cout << "meshes (" << doc.meshes().size() << "):\n";
        for (const auto& mesh : doc.meshes()) {
            std::cout << "  mesh '" << mesh.name << "' submeshes=" << mesh.submeshes.size() << '\n';
            for (const auto& sub : mesh.submeshes) {
                std::cout << "    submesh '" << sub.name << "'\n";
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
