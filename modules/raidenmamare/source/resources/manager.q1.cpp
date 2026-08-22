#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/texpack.q1.h>

#include <filesystem>

namespace rmmr::resource {

    using namespace fqsm::api;

    auto Manager::Actions::singleton(Reading context) -> optional<Id> {
        return with<system::Core>::singleton(context);
    }

    void Manager::Actions::load(Writing context) {
        for (const auto [id, _] : context->aspect<texpack::LoaderCatalog>().items())
            texpack::LoaderCatalog::Actions::load(context, id);
        for (const auto [id, _] : context->aspect<sprite::LoaderKenney>().items())
            sprite::LoaderKenney::Actions::load(context, id);
        for (const auto [id, _] : context->aspect<meshpack::LoaderObjs>().items())
            meshpack::LoaderObjs::Actions::load(context, id);
        for (const auto [id, _] : context->aspect<meshpack::LoaderLwo>().items())
            meshpack::LoaderLwo::Actions::load(context, id);
    }

    auto Manager::Actions::resolve(Reading context, const Unit::Quantum& unit, const filename& relative) -> filepath {
        const auto& manager = with<Manager>::get(context, *singleton(context));
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
