#include "fittings/mounts/catalog.h"

#include <eltanin/resources/assets.q1.h>

#include <base/logging.h>

#include <filesystem>

namespace eltanin {

    using namespace fqsm::api;
    using namespace rmmr::resource;

    void MountCatalog::bind(filepath path) {
        root = std::move(path);
        ids.clear();
        ready = false;
    }

    auto MountCatalog::loadOne(Writing context, string stem) -> base::maybe<mech::Mount::Id> {
        const auto unitName = Unit::Name::from("Eltanin", string{"mounts."} + stem);
        if (const auto existing = with<Assets>::find<mech::Mount>(context, unitName))
            return *existing;
        const auto relative = filename{(std::filesystem::path{"fittings"} / "mounts" / (stem + ".json")).generic_string()};
        const auto assetId = with<::eltanin::resource::Assets>::add_mount(context, unitName, relative);
        if (not context.workers_interface().summary().good())
            return {};
        return assetId;
    }

    void MountCatalog::loadFromDisk(establish::Realm& world) {
        if (ready)
            return;
        ready = true;
        if (root.empty()) {
            base::message("eltanin::MountCatalog::loadFromDisk: root unset");
            return;
        }
        if (not std::filesystem::is_directory(root)) {
            base::message("eltanin::MountCatalog::loadFromDisk: not a directory '{}'", root.string());
            return;
        }
        for (const auto& entry : std::filesystem::directory_iterator(root)) {
            if (not entry.is_regular_file() or entry.path().extension() != ".json")
                continue;
            const auto stem = entry.path().stem().string();
            base::maybe<mech::Mount::Id> loadedId;
            world.branch([&](Writing context) {
                loadedId = loadOne(context, stem);
            });
            if (world.result().good() and loadedId)
                ids.push_back(*loadedId);
            else
                base::message("eltanin::MountCatalog::loadFromDisk: skip '{}'", entry.path().generic_string());
        }
    }

}
