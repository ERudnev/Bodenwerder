#include "blueprints/catalog.h"

#include <eltanin/resources/assets.q1.h>

#include <base/logging.h>

#include <cctype>
#include <filesystem>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr::resource;

    namespace {

        auto trimAscii(std::string_view text) -> std::string_view {
            while (not text.empty() and std::isspace(static_cast<unsigned char>(text.front())))
                text.remove_prefix(1);
            while (not text.empty() and std::isspace(static_cast<unsigned char>(text.back())))
                text.remove_suffix(1);
            return text;
        }

        auto fileStemFromName(std::string_view name) -> string {
            string stem;
            stem.reserve(name.size());
            for (const unsigned char ch : name) {
                if (std::isalnum(ch) or ch == '_' or ch == '-')
                    stem.push_back(static_cast<char>(ch));
                else if (std::isspace(ch) or ch == '.')
                    stem.push_back('_');
            }
            while (not stem.empty() and stem.front() == '_')
                stem.erase(stem.begin());
            while (not stem.empty() and stem.back() == '_')
                stem.pop_back();
            return stem;
        }

        auto shelfFolder(BlueprintShelf shelf) -> const char* {
            switch (shelf) {
                case BlueprintShelf::ships: return "ships";
                case BlueprintShelf::prefabs: return "prefabs";
            }
            return "ships";
        }

        auto unitOwn(BlueprintShelf shelf, const string& stem) -> string {
            return string{shelfFolder(shelf)} + "." + stem;
        }

        auto relativeFile(BlueprintShelf shelf, const string& stem) -> filename {
            return filename{(std::filesystem::path{"blueprints"} / shelfFolder(shelf) / (stem + ".blueprint")).generic_string()};
        }

        auto packFor(BlueprintCatalog& catalog, BlueprintShelf shelf) -> BlueprintIds& {
            switch (shelf) {
                case BlueprintShelf::ships: return catalog.ships;
                case BlueprintShelf::prefabs: return catalog.prefabs;
            }
            return catalog.ships;
        }

        void loadShelf(BlueprintCatalog& catalog, establish::Realm& world, BlueprintShelf shelf) {
            const auto folder = catalog.root / shelfFolder(shelf);
            if (not std::filesystem::is_directory(folder))
                return;
            for (const auto& entry : std::filesystem::directory_iterator(folder)) {
                if (not entry.is_regular_file() or entry.path().extension() != ".blueprint")
                    continue;
                const auto stem = entry.path().stem().string();
                base::maybe<mech::Blueprint::Id> loadedId;
                world.branch([&](Writing context) {
                    loadedId = catalog.loadOne(context, shelf, stem);
                });
                if (world.result().good() and loadedId)
                    packFor(catalog, shelf).push_back(*loadedId);
                else
                    base::message("eltanin::BlueprintCatalog::loadFromDisk: skip '{}'", entry.path().generic_string());
            }
        }

    } // namespace

    void BlueprintCatalog::bind(filepath path) {
        root = std::move(path);
        ships.clear();
        prefabs.clear();
        unnamed.reset();
        newName = {};
        ready = false;
    }

    void BlueprintCatalog::loadFromDisk(establish::Realm& world) {
        if (ready)
            return;
        ready = true;
        if (root.empty()) {
            base::message("eltanin::BlueprintCatalog::loadFromDisk: root unset");
            return;
        }
        if (not std::filesystem::is_directory(root)) {
            base::message("eltanin::BlueprintCatalog::loadFromDisk: not a directory '{}'", root.string());
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(root / "ships", ec);
        std::filesystem::create_directories(root / "prefabs", ec);

        world.branch([&](Writing context) {
            unnamed = ensureUnnamed(context);
        });
        if (not world.result().good() or not unnamed)
            base::message("eltanin::BlueprintCatalog::loadFromDisk: failed to ensure '_unnamed'");

        loadShelf(*this, world, BlueprintShelf::ships);
        loadShelf(*this, world, BlueprintShelf::prefabs);
    }

    auto BlueprintCatalog::ensureUnnamed(Writing context) -> base::maybe<mech::Blueprint::Id> {
        constexpr std::string_view stem = "_unnamed";
        const auto unitName = Unit::Name::from("Eltanin", string{stem});
        if (const auto existing = with<Assets>::find<::eltanin::mech::Blueprint>(context, unitName))
            return *existing;

        const auto relative = filename{(std::filesystem::path{"blueprints"} / (string{stem} + ".blueprint")).generic_string()};
        const auto filePath = root / (string{stem} + ".blueprint");
        const auto assetId = with<::eltanin::resource::Assets>::add_blueprint(context, unitName, relative);
        if (not context.workers_interface().summary().good())
            return {};
        if (not std::filesystem::exists(filePath)) {
            auto writable = with<::eltanin::mech::Blueprint>::modify(context, assetId);
            writable->name = string{stem};
            writable->author = "#undefined";
            with<::eltanin::mech::Blueprint>::save(context, assetId);
            if (not context.workers_interface().summary().good())
                return {};
        }
        return assetId;
    }

    auto BlueprintCatalog::loadOne(Writing context, BlueprintShelf shelf, string stem) -> base::maybe<mech::Blueprint::Id> {
        const auto assetId = with<::eltanin::resource::Assets>::add_blueprint(context, Unit::Name::from("Eltanin", unitOwn(shelf, stem)), relativeFile(shelf, stem));
        if (not context.workers_interface().summary().good())
            return {};
        return assetId;
    }

    auto BlueprintCatalog::createNew(Writing context, BlueprintShelf shelf, std::string_view rawName) -> base::maybe<mech::Blueprint::Id> {
        const auto name = string{trimAscii(rawName)};
        const auto stem = fileStemFromName(name);
        if (stem.empty()) {
            base::message("eltanin::BlueprintCatalog::createNew: name is empty");
            return {};
        }
        if (root.empty()) {
            base::message("eltanin::BlueprintCatalog::createNew: root unset");
            return {};
        }
        const auto own = unitOwn(shelf, stem);
        if (with<Assets>::find<::eltanin::mech::Blueprint>(context, Unit::Name::from("Eltanin", own))) {
            base::message("eltanin::BlueprintCatalog::createNew: unit 'Eltanin::{}' already exists", own);
            return {};
        }
        const auto folder = root / shelfFolder(shelf);
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        const auto filePath = folder / (stem + ".blueprint");
        if (std::filesystem::exists(filePath)) {
            base::message("eltanin::BlueprintCatalog::createNew: file already exists '{}'", filePath.string());
            return {};
        }
        const auto relative = relativeFile(shelf, stem);
        const auto assetId = with<::eltanin::resource::Assets>::add_blueprint(context, Unit::Name::from("Eltanin", own), relative);
        if (not context.workers_interface().summary().good())
            return {};
        {
            auto writable = with<::eltanin::mech::Blueprint>::modify(context, assetId);
            writable->name = name;
            writable->author = "#unknown";
            writable->cells = {};
        }
        with<::eltanin::mech::Blueprint>::save(context, assetId);
        if (not context.workers_interface().summary().good())
            return {};
        packFor(*this, shelf).push_back(assetId);
        return assetId;
    }

}
