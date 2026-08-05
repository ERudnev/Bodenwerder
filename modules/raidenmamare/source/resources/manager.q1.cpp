#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/sprites.q1.h>

#include <filesystem>
#include <stdexcept>

namespace rmmr::resource {

    using namespace fqsm::api;

    auto Manager::Actions::singleton(Reading context) -> optional<Id> {
        return with<system::Core>::singleton(context);
    }

    void Manager::Actions::load(Writing context) {
        if (not singleton(context)) return;
        for (const auto [id, _] : context->aspect<sprite::LoaderKenney>().items()) {
            sprite::LoaderKenney::Actions::load(context, id);
            if (not context.workers_interface().summary().good())
                return;
        }
        for (const auto [id, _] : context->aspect<meshpack::LoaderObjs>().items()) {
            meshpack::LoaderObjs::Actions::load(context, id);
            if (not context.workers_interface().summary().good())
                return;
        }
        for (const auto [id, _] : context->aspect<meshpack::LoaderLwo>().items()) {
            meshpack::LoaderLwo::Actions::load(context, id);
            if (not context.workers_interface().summary().good())
                return;
        }
    }

    auto Manager::Actions::resolve(Reading context, const Unit::Quantum& unit, const filename& relative) -> filepath {
        const auto manager_id = singleton(context);
        if (not manager_id)
            throw std::runtime_error("resource::Manager::resolve: singleton missing");
        const auto& manager = with<Manager>::get(context, *manager_id);
        const std::filesystem::path file_path(relative);
        if (file_path.is_absolute())
            return file_path;
        if (unit.name.library.empty())
            return manager.location / file_path;
        return manager.location / unit.name.library / file_path;
    }

    auto Unit::Actions::remember(Reading context, Id id) -> Reference {
        return {.id = id, .backup = with<Unit>::get(context, id).name};
    }

}
