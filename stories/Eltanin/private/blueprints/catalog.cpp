#include "blueprints/catalog.h"

#include <eltanin/resources/assets.q1.h>

#include "mech/blueprint.h"

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

    } // namespace

    void BlueprintCatalog::bind(filepath path) {
        directory = std::move(path);
        items.clear();
        newName = {};
        ready = false;
    }

    void BlueprintCatalog::loadFromDisk(establish::Realm& world) {
        if (ready)
            return;
        ready = true;
        if (directory.empty()) {
            base::message("eltanin::BlueprintCatalog::loadFromDisk: directory unset");
            return;
        }
        if (not std::filesystem::is_directory(directory)) {
            base::message("eltanin::BlueprintCatalog::loadFromDisk: not a directory '{}'", directory.string());
            return;
        }
        bool foundBlueprint = false;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (not entry.is_regular_file() or entry.path().extension() != ".blueprint")
                continue;
            foundBlueprint = true;
            const auto stem = entry.path().stem().string();
            base::maybe<resource::blueprint::Asset::Id> loadedId;
            world.branch([&](Writing context) {
                loadedId = loadOne(context, stem);
            });
            if (world.result().good() and loadedId)
                items.push_back(*loadedId);
            else
                base::message("eltanin::BlueprintCatalog::loadFromDisk: skip '{}'", entry.path().filename().string());
        }
        if (foundBlueprint)
            return;

        world.branch([&](Writing context) {
            constexpr std::string_view stem = "_unnamed";
            constexpr std::string_view shipName = "_unnamed";
            constexpr std::string_view manufacturer = "#undefined";
            const auto relative = filename{(std::filesystem::path{"blueprints"} / (string{stem} + ".blueprint")).generic_string()};
            const auto assetId = with<::eltanin::resource::Assets>::add_blueprint_loader(context, Unit::Name::from("Eltanin", string{stem}), item<::eltanin::resource::blueprint::Loader>{.file = relative});
            with<::eltanin::resource::blueprint::Asset>::modify(context, assetId)->data = mech::Blueprint{.name = string{shipName}, .author = string{manufacturer}, .knots = {}};
            with<::eltanin::resource::blueprint::Loader>::save(context, assetId);
            if (not context.workers_interface().summary().good())
                return;
            items.push_back(assetId);
        });
        if (items.empty())
            base::message("eltanin::BlueprintCatalog::loadFromDisk: failed to create default '{}'", (directory / "_unnamed.blueprint").string());
    }

    auto BlueprintCatalog::loadOne(Writing context, string stem) -> base::maybe<resource::blueprint::Asset::Id> {
        const auto relative = filename{(std::filesystem::path{"blueprints"} / (stem + ".blueprint")).generic_string()};
        const auto assetId = with<::eltanin::resource::Assets>::add_blueprint_loader(context, Unit::Name::from("Eltanin", stem), item<::eltanin::resource::blueprint::Loader>{.file = relative});
        with<::eltanin::resource::blueprint::Loader>::load(context, assetId);
        if (not context.workers_interface().summary().good())
            return {};
        return assetId;
    }

    auto BlueprintCatalog::createNew(Writing context, std::string_view rawName) -> base::maybe<resource::blueprint::Asset::Id> {
        const auto name = string{trimAscii(rawName)};
        const auto stem = fileStemFromName(name);
        if (stem.empty()) {
            base::message("eltanin::BlueprintCatalog::createNew: name is empty");
            return {};
        }
        if (directory.empty()) {
            base::message("eltanin::BlueprintCatalog::createNew: directory unset");
            return {};
        }
        if (with<Assets>::find<::eltanin::resource::blueprint::Asset>(context, Unit::Name::from("Eltanin", stem))) {
            base::message("eltanin::BlueprintCatalog::createNew: unit 'Eltanin::{}' already exists", stem);
            return {};
        }
        const auto filePath = directory / (stem + ".blueprint");
        if (std::filesystem::exists(filePath)) {
            base::message("eltanin::BlueprintCatalog::createNew: file already exists '{}'", filePath.string());
            return {};
        }
        const auto relative = filename{(std::filesystem::path{"blueprints"} / (stem + ".blueprint")).generic_string()};
        const auto assetId = with<::eltanin::resource::Assets>::add_blueprint_loader(context, Unit::Name::from("Eltanin", stem), item<::eltanin::resource::blueprint::Loader>{.file = relative});
        with<::eltanin::resource::blueprint::Asset>::modify(context, assetId)->data = mech::Blueprint{.name = name, .author = "#unknown", .knots = {}};
        with<::eltanin::resource::blueprint::Loader>::save(context, assetId);
        if (not context.workers_interface().summary().good())
            return {};
        items.push_back(assetId);
        return assetId;
    }

}
