#include <eltanin/resources/assets.q1.h>

#include <rmmr/resources/manager.q1.h>

#include <filesystem>

namespace eltanin::resource {

    using namespace fqsm::api;

    auto Assets::Actions::add_blueprint(Writing context, rmmr::resource::Unit::Name name, filename file) -> mech::Blueprint::Id {
        const auto assets = with<rmmr::resource::Assets>::singleton(context);
        if (not assets)
            return context.refuse("eltanin::resource::Assets: rmmr Assets singleton missing");
        if (not with<rmmr::resource::Unit_group>::exists(context, *assets))
            with<rmmr::resource::Unit_group>::extend(context, *assets);
        const auto unitId = with<rmmr::resource::Unit_group>::addElement(context, *assets, rmmr::resource::Unit::Quantum{.name = std::move(name)});
        with<mech::Blueprint>::extend(context, unitId, mech::Blueprint::Quantum{.name = {}, .author = {}, .cells = {}, .file = std::move(file)});
        const auto& unit = with<rmmr::resource::Unit>::get(context, unitId);
        const auto& blueprint = with<mech::Blueprint>::get(context, unitId);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, blueprint.file);
        if (std::filesystem::exists(path))
            with<mech::Blueprint>::load(context, unitId);
        return unitId;
    }

}
