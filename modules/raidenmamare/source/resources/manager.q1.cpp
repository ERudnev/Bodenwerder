#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/sprites.q1.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    auto Manager::Actions::singleton(Reading context) -> optional<Id> {
        return with<system::Core>::singleton(context);
    }

    void Manager::Actions::load(Writing context) {
        if (not singleton(context)) return;
        for (const auto [id, _] : context->aspect<sprite::LoaderKenney>().items()) {
            sprite::LoaderKenney::Actions::load(context, id);
        }
        for (const auto [id, _] : context->aspect<meshpack::Loader>().items()) {
            meshpack::Loader::Actions::load(context, id);
        }
    }

    auto Unit::Actions::remember(Reading context, Id id) -> Reference {
        return {.id = id, .backup = with<Unit>::get(context, id).name};
    }

}
