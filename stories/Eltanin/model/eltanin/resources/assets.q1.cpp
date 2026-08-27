#include <eltanin/resources/assets.q1.h>

#include <rmmr/resources/manager.q1.h>

#include <filesystem>

namespace eltanin::resource {

    using namespace fqsm::api;

    namespace {

        template <typename Feature>
        auto registerAndMaybeLoad(Writing context, rmmr::resource::Unit::Name name, filename file, typename Feature::Quantum quantum) -> typename Feature::Id {
            const auto assets = with<rmmr::resource::Assets>::singleton(context);
            quantum.file = std::move(file);
            const auto unitId = with<rmmr::resource::Unit_group>::addElement(context, assets, rmmr::resource::Unit::Quantum{.name = std::move(name)});
            with<Feature>::extend(context, unitId, std::move(quantum));
            const auto& unit = with<rmmr::resource::Unit>::get(context, unitId);
            const auto& asset = with<Feature>::get(context, unitId);
            const auto path = with<rmmr::resource::Manager>::resolve(context, unit, asset.file);
            if (std::filesystem::exists(path))
                with<Feature>::load(context, unitId);
            return unitId;
        }

    } // namespace

    auto Assets::Actions::add_blueprint(Writing context, rmmr::resource::Unit::Name name, filename file) -> mech::Blueprint::Id {
        return registerAndMaybeLoad<mech::Blueprint>(context, std::move(name), std::move(file), mech::Blueprint::Quantum{.name = {}, .author = {}, .cells = {}, .mounts = {}, .file = {}});
    }

    auto Assets::Actions::add_mount(Writing context, rmmr::resource::Unit::Name name, filename file) -> mech::Mount::Id {
        return registerAndMaybeLoad<mech::Mount>(
            context,
            std::move(name),
            std::move(file),
            mech::Mount::Quantum{
                .name = {},
                .author = {},
                .mass = 0.0f,
                .attachment = mech::Attachment{.points = {}},
                .collision = mech::Collision{.thickness = 0.0f, .faces = {}},
                .tempMesh = mech::Mount::TempMesh{.pack = {}, .entry = {}},
                .role = {},
                .file = {},
            });
    }

}
