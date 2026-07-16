#include <rmmr/resources/runtimes.q1.h>

#include <format>

namespace rmmr::resource {

    using namespace fqsm::api;

    namespace {

        template<typename Runtime, typename Group, typename AssetId>
        void rebuild_runtime(Writing context, system::Device::Id device, umap<AssetId, typename Runtime::Id>& mapping, AssetId asset_id, typename Runtime::Quantum runtime) {
            if (const auto existing = mapping.find(asset_id); existing != mapping.end()) {
                with<Runtime>::remove(context, existing->second);
                mapping.erase(asset_id);
            }
            const auto runtime_id = with<Group>::addElement(context, device, std::move(runtime));
            mapping.emplace(asset_id, runtime_id);
        }

        template<typename Asset, typename Runtime>
        void scrub_mapping(Reacting context, Runtimes::Id runtimes_id, const umap<typename Asset::Id, typename Runtime::Id>& mapping, umap<typename Asset::Id, typename Runtime::Id> Runtimes::Quantum::* field) {
            auto& runtime_patch = context.reaction<Runtime>();
            auto& runtimes_patch = context.reaction<Runtimes>();
            for (const auto& [asset_id, runtime_id] : mapping) {
                const bool asset_exists = with<Asset>::exists(context, asset_id);
                const bool runtime_exists = with<Runtime>::exists(context, runtime_id);
                if (asset_exists && runtime_exists) {
                    continue;
                }
                if (runtime_exists) {
                    runtime_patch.put_deletion(runtime_id);
                }
                auto& fixed = runtimes_patch.update_modification(runtimes_id, [&]() -> const Runtimes::Quantum& {
                    return with<Runtimes>::get(context, runtimes_id);
                });
                (fixed.*field).erase(asset_id);
            }
        }

    } // namespace

    auto Assets::Actions::add_texture_file(Writing context, Id assets, Unit::Quantum unit, texture::Asset::Quantum asset, texture::FromFile::Quantum from_file) -> texture::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<texture::Asset>::extend(context, unit_id, std::move(asset));
        with<texture::FromFile>::extend(context, unit_id, std::move(from_file));
        return unit_id;
    }

    auto Assets::Actions::add_texture_generated(Writing context, Id assets, Unit::Quantum unit, texture::Asset::Quantum asset, texture::Generated::Quantum generated) -> texture::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<texture::Asset>::extend(context, unit_id, std::move(asset));
        with<texture::Generated>::extend(context, unit_id, std::move(generated));
        return unit_id;
    }

    auto Assets::Actions::add_shader_file(Writing context, Id assets, Unit::Quantum unit, shader::Asset::Quantum asset, shader::FromFile::Quantum from_file) -> shader::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<shader::Asset>::extend(context, unit_id, std::move(asset));
        with<shader::FromFile>::extend(context, unit_id, std::move(from_file));
        return unit_id;
    }

    auto Assets::Actions::add_material(Writing context, Id assets, Unit::Quantum unit, material::Asset::Quantum asset, material::Composed::Quantum composed) -> material::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<material::Asset>::extend(context, unit_id, std::move(asset));
        with<material::Composed>::extend(context, unit_id, std::move(composed));
        return unit_id;
    }

    auto Assets::Actions::add_shadow_allocated(Writing context, Id assets, Unit::Quantum unit, shadow::Asset::Quantum asset, shadow::Allocated::Quantum allocated) -> shadow::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<shadow::Asset>::extend(context, unit_id, std::move(asset));
        with<shadow::Allocated>::extend(context, unit_id, std::move(allocated));
        return unit_id;
    }

    void Assets::Actions::extend(Writing context, Manager::Id manager, filepath path) {
        with<Manager>::modify(context, manager)->location = std::move(path);

        const auto debug_texture = add_texture_file(
            context,
            manager,
            Unit::Quantum{
                .manager = manager,
                .name = "debug_texture",
                .library = "rmmr",
            },
            texture::Asset::Quantum{},
            texture::FromFile::Quantum{
                .file = "textures/debug01.jpg",
            });

        if (not with<Assets>::exists(context, manager)) {
            BaseActions::extend(context, manager, Quantum{
                .debug_texture = debug_texture,
            });
            return;
        }

        with<Assets>::modify(context, manager)->debug_texture = debug_texture;
    }

    void Runtimes::Actions::install(Writing context, Id device) {
        if (with<Runtimes>::exists(context, device)) return (void)context.refuse(std::format("resource::Runtimes::install: already installed for device {}", device));

        with<DeviceRuntimes>::extend(context, device, DeviceRuntimes::Quantum{
            .assets = with<system::Device>::get(context, device).core,
        });
        with<Runtime_group>::extend(context, device);
        with<ShaderRuntime_group>::extend(context, device);
        with<MaterialRuntime_group>::extend(context, device);
        with<ShadowRuntime_group>::extend(context, device);
        BaseActions::extend(context, device, Quantum{});
    }

    void Runtimes::Actions::materialize(Writing context, Id device, Assets::Id assets) {
        with<DeviceRuntimes>::modify(context, device)->assets = assets;
        auto runtimes = with<Runtimes>::modify(context, device);

        for (const auto entry : context->aspect<texture::FromFile>().items()) {
            if (with<Unit>::get(context, entry.id).manager != assets) continue;
            rebuild_runtime<texture::Runtime, Runtime_group>(context, device, runtimes->textures_id_mapping, entry.id, texture::FromFile::Actions::materialize(context, entry.id, device));
        }

        for (const auto entry : context->aspect<texture::Generated>().items()) {
            if (with<Unit>::get(context, entry.id).manager != assets) continue;
            rebuild_runtime<texture::Runtime, Runtime_group>(context, device, runtimes->textures_id_mapping, entry.id, texture::Generated::Actions::materialize(context, entry.id, device));
        }

        for (const auto entry : context->aspect<shader::FromFile>().items()) {
            if (with<Unit>::get(context, entry.id).manager != assets) continue;
            const auto runtime = shader::FromFile::Actions::materialize(context, entry.id, device);
            if (runtime.handle) {
                rebuild_runtime<shader::Runtime, ShaderRuntime_group>(context, device, runtimes->shaders_id_mapping, entry.id, runtime);
            }
        }

        for (const auto entry : context->aspect<material::Composed>().items()) {
            if (with<Unit>::get(context, entry.id).manager != assets) continue;
            rebuild_runtime<material::Runtime, MaterialRuntime_group>(context, device, runtimes->materials_id_mapping, entry.id, material::Composed::Actions::materialize(context, entry.id, device));
        }

        for (const auto entry : context->aspect<shadow::Allocated>().items()) {
            if (with<Unit>::get(context, entry.id).manager != assets) continue;
            const auto runtime = shadow::Allocated::Actions::materialize(context, entry.id, device);
            if (runtime.fbo) {
                rebuild_runtime<shadow::Runtime, ShadowRuntime_group>(context, device, runtimes->shadows_id_mapping, entry.id, runtime);
            }
        }
    }

    struct Runtimes::Internals : Runtimes::DefaultInternals {
        static void maintain_all_mappings(Reacting context) {
            for (const auto entry : context.proposal.aspect<Runtimes>().items()) {
                scrub_mapping<texture::Asset, texture::Runtime>(context, entry.id, entry.value.textures_id_mapping, &Quantum::textures_id_mapping);
                scrub_mapping<shader::Asset, shader::Runtime>(context, entry.id, entry.value.shaders_id_mapping, &Quantum::shaders_id_mapping);
                scrub_mapping<material::Asset, material::Runtime>(context, entry.id, entry.value.materials_id_mapping, &Quantum::materials_id_mapping);
                scrub_mapping<shadow::Asset, shadow::Runtime>(context, entry.id, entry.value.shadows_id_mapping, &Quantum::shadows_id_mapping);
            }
        }
    };

    auto Runtimes::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Runtimes, Assets>(&Runtimes::Internals::maintain_all_mappings),
        };
    }

}
