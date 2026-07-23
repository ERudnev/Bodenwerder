#include <rmmr/resources/runtimes.q1.h>

#include <format>

namespace rmmr::resource {

    using namespace fqsm::api;

    namespace {

        template<typename Asset, typename Kind>
        auto register_unit(Writing context, Assets::Id assets, Unit::Quantum unit, typename Asset::Quantum asset, typename Kind::Quantum kind) -> typename Asset::Id {
            unit.manager = assets;
            if (not with<Unit_group>::exists(context, assets)) {
                with<Unit_group>::extend(context, assets);
            }
            const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
            with<Asset>::extend(context, unit_id, std::move(asset));
            with<Kind>::extend(context, unit_id, std::move(kind));
            return unit_id;
        }

        template<typename Runtime, typename AssetId>
        void bind_runtime(umap<AssetId, typename Runtime::Id>& mapping, AssetId asset_id, optional<typename Runtime::Id> runtime_id) {
            if (not runtime_id) {
                return;
            }
            mapping.insert_or_assign(asset_id, *runtime_id);
        }

        template<typename Fn>
        void for_devices_of_assets(Writing context, Assets::Id assets, Fn&& fn) {
            for (const auto [device, host] : context->aspect<DeviceRuntimes>().items()) {
                if (host.assets != assets) continue;
                if (not with<Runtimes>::exists(context, device)) continue;
                fn(device);
            }
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

        // God knows Kind types; finds handler by Asset id (Kind::Id === Asset::Id).
        void rematerialize_texture(Writing context, texture::Asset::Id asset_id, system::Device::Id device) {
            auto& mapping = with<Runtimes>::modify(context, device)->textures_id_mapping;
            if (with<texture::Loader>::exists(context, asset_id)) {
                bind_runtime<texture::Runtime>(mapping, asset_id, texture::Loader::Actions::materialize(context, asset_id, device));
            } else if (with<texture::Generator>::exists(context, asset_id)) {
                bind_runtime<texture::Runtime>(mapping, asset_id, texture::Generator::Actions::materialize(context, asset_id, device));
            }
        }

        void rematerialize_shader(Writing context, shader::Asset::Id asset_id, system::Device::Id device) {
            if (not with<shader::Loader>::exists(context, asset_id)) return;
            bind_runtime<shader::Runtime>(
                with<Runtimes>::modify(context, device)->shaders_id_mapping,
                asset_id,
                shader::Loader::Actions::materialize(context, asset_id, device));
        }

        void rematerialize_material(Writing context, material::Asset::Id asset_id, system::Device::Id device) {
            if (not with<material::Composer>::exists(context, asset_id)) return;
            bind_runtime<material::Runtime>(
                with<Runtimes>::modify(context, device)->materials_id_mapping,
                asset_id,
                material::Composer::Actions::materialize(context, asset_id, device));
        }

        void rematerialize_shadow(Writing context, shadow::Asset::Id asset_id, system::Device::Id device) {
            if (not with<shadow::Allocator>::exists(context, asset_id)) return;
            bind_runtime<shadow::Runtime>(
                with<Runtimes>::modify(context, device)->shadows_id_mapping,
                asset_id,
                shadow::Allocator::Actions::materialize(context, asset_id, device));
        }

        void rematerialize_geometry(Writing context, geometry::Asset::Id asset_id, system::Device::Id device) {
            auto& mapping = with<Runtimes>::modify(context, device)->geometries_id_mapping;
            if (with<geometry::Loader>::exists(context, asset_id)) {
                bind_runtime<geometry::Runtime>(mapping, asset_id, geometry::Loader::Actions::materialize(context, asset_id, device));
            } else if (with<geometry::Generator>::exists(context, asset_id)) {
                bind_runtime<geometry::Runtime>(mapping, asset_id, geometry::Generator::Actions::materialize(context, asset_id, device));
            }
        }

        void rematerialize_materials_using_texture(Writing context, texture::Asset::Id texture_id, system::Device::Id device) {
            const auto mapping = with<Runtimes>::get(context, device).materials_id_mapping;
            for (const auto& [material_id, _] : mapping) {
                if (not with<material::Asset>::exists(context, material_id)) continue;
                bool uses_texture = false;
                for (const auto& [_, technique] : with<material::Asset>::get(context, material_id).techniques) {
                    for (const auto& binding : technique.textures) {
                        if (binding.texture.id == texture_id) {
                            uses_texture = true;
                            break;
                        }
                    }
                    if (uses_texture) break;
                }
                if (uses_texture) {
                    rematerialize_material(context, material_id, device);
                }
            }
        }

        void rematerialize_materials_using_shader(Writing context, shader::Asset::Id shader_id, system::Device::Id device) {
            const auto mapping = with<Runtimes>::get(context, device).materials_id_mapping;
            for (const auto& [material_id, _] : mapping) {
                if (not with<material::Asset>::exists(context, material_id)) continue;
                bool uses_shader = false;
                for (const auto& [_, technique] : with<material::Asset>::get(context, material_id).techniques) {
                    if (technique.program.id == shader_id) {
                        uses_shader = true;
                        break;
                    }
                }
                if (uses_shader) {
                    rematerialize_material(context, material_id, device);
                }
            }
        }

    } // namespace

    auto Assets::Actions::add_texture_loader(Writing context, Id assets, Unit::Quantum unit, texture::Asset::Quantum asset, texture::Loader::Quantum loader) -> texture::Asset::Id {
        return register_unit<texture::Asset, texture::Loader>(context, assets, std::move(unit), std::move(asset), std::move(loader));
    }

    auto Assets::Actions::add_texture_generator(Writing context, Id assets, Unit::Quantum unit, texture::Asset::Quantum asset, texture::Generator::Quantum generator) -> texture::Asset::Id {
        return register_unit<texture::Asset, texture::Generator>(context, assets, std::move(unit), std::move(asset), std::move(generator));
    }

    auto Assets::Actions::add_shader_loader(Writing context, Id assets, Unit::Quantum unit, shader::Asset::Quantum asset, shader::Loader::Quantum loader) -> shader::Asset::Id {
        return register_unit<shader::Asset, shader::Loader>(context, assets, std::move(unit), std::move(asset), std::move(loader));
    }

    auto Assets::Actions::add_material(Writing context, Id assets, Unit::Quantum unit, material::Asset::Quantum asset, material::Composer::Quantum composer) -> material::Asset::Id {
        return register_unit<material::Asset, material::Composer>(context, assets, std::move(unit), std::move(asset), std::move(composer));
    }

    auto Assets::Actions::add_shadow_allocator(Writing context, Id assets, Unit::Quantum unit, shadow::Asset::Quantum asset, shadow::Allocator::Quantum allocator) -> shadow::Asset::Id {
        return register_unit<shadow::Asset, shadow::Allocator>(context, assets, std::move(unit), std::move(asset), std::move(allocator));
    }

    auto Assets::Actions::add_geometry_loader(Writing context, Id assets, Unit::Quantum unit, geometry::Asset::Quantum asset, geometry::Loader::Quantum loader) -> geometry::Asset::Id {
        return register_unit<geometry::Asset, geometry::Loader>(context, assets, std::move(unit), std::move(asset), std::move(loader));
    }

    auto Assets::Actions::add_geometry_generator(Writing context, Id assets, Unit::Quantum unit, geometry::Asset::Quantum asset, geometry::Generator::Quantum generator) -> geometry::Asset::Id {
        return register_unit<geometry::Asset, geometry::Generator>(context, assets, std::move(unit), std::move(asset), std::move(generator));
    }

    void Assets::Actions::extend(Writing context, Manager::Id manager, filepath path) {
        with<Manager>::modify(context, manager)->location = std::move(path);

        if (not with<Assets>::exists(context, manager)) {
            BaseActions::extend(context, manager, Quantum{});
        }
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
        with<GeometryRuntime_group>::extend(context, device);
        BaseActions::extend(context, device, Quantum{});
    }

    void Runtimes::Actions::materialize(Writing context, Id device, Assets::Id assets) {
        with<DeviceRuntimes>::modify(context, device)->assets = assets;

        // Inventory = family Asset tables; handler = Kind found by same id. Family order: deps before dependents.
        for (const auto [id, _] : context->aspect<texture::Asset>().items()) {
            if (with<Unit>::get(context, id).manager != assets) continue;
            rematerialize_texture(context, id, device);
        }
        for (const auto [id, _] : context->aspect<shader::Asset>().items()) {
            if (with<Unit>::get(context, id).manager != assets) continue;
            rematerialize_shader(context, id, device);
        }
        for (const auto [id, _] : context->aspect<material::Asset>().items()) {
            if (with<Unit>::get(context, id).manager != assets) continue;
            rematerialize_material(context, id, device);
        }
        for (const auto [id, _] : context->aspect<shadow::Asset>().items()) {
            if (with<Unit>::get(context, id).manager != assets) continue;
            rematerialize_shadow(context, id, device);
        }
        for (const auto [id, _] : context->aspect<geometry::Asset>().items()) {
            if (with<Unit>::get(context, id).manager != assets) continue;
            rematerialize_geometry(context, id, device);
        }
    }

    struct Runtimes::Internals : Runtimes::DefaultInternals {
        static void maintain_all_mappings(Reacting context) {
            for (const auto [runtimes_id, quantum] : context.proposal.aspect<Runtimes>().items()) {
                scrub_mapping<texture::Asset, texture::Runtime>(context, runtimes_id, quantum.textures_id_mapping, &Quantum::textures_id_mapping);
                scrub_mapping<shader::Asset, shader::Runtime>(context, runtimes_id, quantum.shaders_id_mapping, &Quantum::shaders_id_mapping);
                scrub_mapping<material::Asset, material::Runtime>(context, runtimes_id, quantum.materials_id_mapping, &Quantum::materials_id_mapping);
                scrub_mapping<shadow::Asset, shadow::Runtime>(context, runtimes_id, quantum.shadows_id_mapping, &Quantum::shadows_id_mapping);
                scrub_mapping<geometry::Asset, geometry::Runtime>(context, runtimes_id, quantum.geometries_id_mapping, &Quantum::geometries_id_mapping);
            }
        }

        static void align_from_assets(Reacting context) {
            Writing writing = context;

            for (const auto change : context.changes<texture::Asset>().addedOrUpdated()) {
                if (not with<Unit>::exists(writing, change.id)) continue;
                const auto assets = with<Unit>::get(writing, change.id).manager;
                for_devices_of_assets(writing, assets, [&](system::Device::Id device) {
                    rematerialize_texture(writing, change.id, device);
                    rematerialize_materials_using_texture(writing, change.id, device);
                });
            }

            for (const auto change : context.changes<shader::Asset>().addedOrUpdated()) {
                if (not with<Unit>::exists(writing, change.id)) continue;
                const auto assets = with<Unit>::get(writing, change.id).manager;
                for_devices_of_assets(writing, assets, [&](system::Device::Id device) {
                    rematerialize_shader(writing, change.id, device);
                    rematerialize_materials_using_shader(writing, change.id, device);
                });
            }

            for (const auto change : context.changes<material::Asset>().addedOrUpdated()) {
                if (not with<Unit>::exists(writing, change.id)) continue;
                const auto assets = with<Unit>::get(writing, change.id).manager;
                for_devices_of_assets(writing, assets, [&](system::Device::Id device) {
                    rematerialize_material(writing, change.id, device);
                });
            }

            for (const auto change : context.changes<shadow::Asset>().addedOrUpdated()) {
                if (not with<Unit>::exists(writing, change.id)) continue;
                const auto assets = with<Unit>::get(writing, change.id).manager;
                for_devices_of_assets(writing, assets, [&](system::Device::Id device) {
                    rematerialize_shadow(writing, change.id, device);
                });
            }

            for (const auto change : context.changes<geometry::Asset>().addedOrUpdated()) {
                if (not with<Unit>::exists(writing, change.id)) continue;
                const auto assets = with<Unit>::get(writing, change.id).manager;
                for_devices_of_assets(writing, assets, [&](system::Device::Id device) {
                    rematerialize_geometry(writing, change.id, device);
                });
            }
        }
    };

    auto Runtimes::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Runtimes, Assets>(&Runtimes::Internals::maintain_all_mappings),
            reaction::aspect_wide<Runtimes, texture::Asset, shader::Asset, material::Asset, shadow::Asset, geometry::Asset>(&Runtimes::Internals::align_from_assets),
        };
    }

}
