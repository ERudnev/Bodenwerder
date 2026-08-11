#include <eltanin/resources/assets.q1.h>

#include <rmmr/resources/manager.q1.h>

namespace eltanin::resource {

    using namespace fqsm::api;

    namespace {

        template<typename Asset, typename Kind>
        auto register_unit(Writing context, rmmr::resource::Unit::Name name, typename Asset::Quantum asset, typename Kind::Quantum kind) -> typename Asset::Id {
            const auto assets = with<rmmr::resource::Assets>::singleton(context);
            if (not assets) {
                return context.refuse("eltanin::resource::Assets: rmmr Assets singleton missing");
            }
            if (not with<rmmr::resource::Unit_group>::exists(context, *assets)) {
                with<rmmr::resource::Unit_group>::extend(context, *assets);
            }
            const auto unit_id = with<rmmr::resource::Unit_group>::addElement(context, *assets, rmmr::resource::Unit::Quantum{.name = std::move(name)});
            with<Asset>::extend(context, unit_id, std::move(asset));
            with<Kind>::extend(context, unit_id, std::move(kind));
            return unit_id;
        }

    } // namespace

    auto Assets::Actions::add_blueprint_loader(Writing context, rmmr::resource::Unit::Name name, blueprint::Loader::Quantum loader) -> blueprint::Asset::Id {
        return register_unit<blueprint::Asset, blueprint::Loader>(
            context,
            std::move(name),
            blueprint::Asset::Quantum{.data = mech::Blueprint{.name = {}, .author = {}, .cells = {}}},
            std::move(loader));
    }

}
