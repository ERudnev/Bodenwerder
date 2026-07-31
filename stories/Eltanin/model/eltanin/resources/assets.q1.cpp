#include <eltanin/resources/assets.q1.h>
#include <rmmr/resources/manager.q1.h>

namespace eltanin::resource {

    using namespace fqsm::api;
    using rmmr::resource::Manager;
    using rmmr::resource::Unit;
    using rmmr::resource::Unit_group;

    namespace {

        template<typename Asset, typename Kind>
        auto register_unit(Writing context, Unit::Quantum unit, typename Asset::Quantum asset, typename Kind::Quantum kind) -> typename Asset::Id {
            const auto host = with<rmmr::resource::Assets>::singleton(context);
            if (not host) return context.refuse("eltanin::resource::Assets: rmmr Assets singleton missing");
            if (not with<Assets>::exists(context, *host)) {
                return context.refuse("eltanin::resource::Assets: missing; create in createCore wave with rmmr::Assets");
            }
            if (not with<Unit_group>::exists(context, *host)) {
                with<Unit_group>::extend(context, *host);
            }
            const auto unit_id = with<Unit_group>::addElement(context, *host, std::move(unit));
            with<Asset>::extend(context, unit_id, std::move(asset));
            with<Kind>::extend(context, unit_id, std::move(kind));
            return unit_id;
        }

    } // namespace

    auto Assets::Actions::add_physical_loader(Writing context, Unit::Quantum unit, physical::Loader::Quantum loader) -> physical::Asset::Id {
        return register_unit<physical::Asset, physical::Loader>(context, std::move(unit), physical::Asset::Quantum{}, std::move(loader));
    }

    void Assets::Actions::load(Writing context) {
        if (not with<rmmr::resource::Assets>::singleton(context)) {
            return (void)context.refuse("eltanin::resource::Assets::load: rmmr Assets singleton missing");
        }
        for (const auto [id, _] : context->aspect<physical::Loader>().items()) {
            with<physical::Loader>::load(context, id);
        }
    }

}
