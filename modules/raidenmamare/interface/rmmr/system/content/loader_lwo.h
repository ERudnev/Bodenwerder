#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rmmr::system::content {

    // Inspector-only structural LWO parser (no fQSM / GPU / logging).
    // Runtime meshpack loading uses geometry::Loader's single Assimp import and its catalogs.
    class LwoDocument {
    public:
        // Submesh local name = material name from the file (plain string, not "lib::own").
        struct Submesh {
            std::string name;
        };

        struct Mesh {
            std::string name; // mesh identity (LightWave layer tag '=…' stripped inside the loader)
            std::vector<Submesh> submeshes;
        };

        struct OpenResult {
            std::unique_ptr<LwoDocument> document;
            std::string error;
        };

        static auto open(const std::filesystem::path& path) -> OpenResult;

        auto path() const -> const std::filesystem::path&;
        auto materials() const -> const std::vector<std::string>&;
        auto meshes() const -> const std::vector<Mesh>&;

        LwoDocument(const LwoDocument&) = delete;
        auto operator=(const LwoDocument&) -> LwoDocument& = delete;
        LwoDocument(LwoDocument&&) noexcept;
        auto operator=(LwoDocument&&) noexcept -> LwoDocument&;
        ~LwoDocument();

    private:
        struct Impl;
        explicit LwoDocument(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> impl_;
    };

}
