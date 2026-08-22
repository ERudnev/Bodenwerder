#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/semantics/rendering.h>

#include <base/logging.h>

#include <format>

namespace rmmr::resource {

    using namespace fqsm::api;

    namespace {

        template<typename Asset, typename Kind>
        auto register_unit(Writing context, Unit::Name name, typename Asset::Quantum asset, typename Kind::Quantum kind) -> typename Asset::Id {
            const auto assets = with<Assets>::singleton(context);
            const auto unit_id = with<Unit_group>::addElement(context, assets, Unit::Quantum{.name = std::move(name)});
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

        template<typename Asset, typename Runtime>
        void scrub_mapping(Reacting context, Runtimes::Id runtimes_id, const umap<typename Asset::Id, typename Runtime::Id>& mapping, umap<typename Asset::Id, typename Runtime::Id> Runtimes::Quantum::* field) {
            auto& runtime_patch = context.adjustments<Runtime>();
            auto& runtimes_patch = context.adjustments<Runtimes>();
            for (const auto& [asset_id, runtime_id] : mapping) {
                const bool asset_exists = with<Asset>::exists(context, asset_id);
                const bool runtime_exists = with<Runtime>::exists(context, runtime_id);
                if (asset_exists && runtime_exists) {
                    continue;
                }
                if (runtime_exists) {
                    runtime_patch.put_deletion(runtime_id);
                }
                auto& fixed = runtimes_patch.get_modification_access(runtimes_id);
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

        void rematerialize_texpack(Writing context, texpack::Pack::Id pack_id, system::Device::Id device) {
            if (not with<texpack::Pack>::exists(context, pack_id)) {
                return;
            }
            bind_runtime<texpack::Runtime>(
                with<Runtimes>::modify(context, device)->texpacks_id_mapping,
                pack_id,
                texpack::Pack::Actions::materialize(context, pack_id, device));
        }

        void rematerialize_shader(Writing context, shader::Asset::Id asset_id, system::Device::Id device) {
            if (not with<shader::Loader>::exists(context, asset_id)) return;
            bind_runtime<shader::Runtime>(
                with<Runtimes>::modify(context, device)->shaders_id_mapping,
                asset_id,
                shader::Loader::Actions::materialize(context, asset_id, device));
        }

        void rematerialize_material(Writing context, material::Asset::Id asset_id, system::Device::Id device) {
            if (not with<material::Asset>::exists(context, asset_id)) return;
            bind_runtime<material::Runtime>(
                with<Runtimes>::modify(context, device)->materials_id_mapping,
                asset_id,
                material::Asset::Actions::materialize(context, asset_id, device));
        }

        void rematerialize_overlay(Writing context, overlay::Asset::Id asset_id, system::Device::Id device) {
            if (not with<overlay::Asset>::exists(context, asset_id)) return;
            bind_runtime<overlay::Runtime>(
                with<Runtimes>::modify(context, device)->overlays_id_mapping,
                asset_id,
                overlay::Asset::Actions::materialize(context, asset_id, device));
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

        void rematerialize_sprites(Writing context, sprite::Pack::Id pack_id, system::Device::Id device) {
            if (not with<sprite::Pack>::exists(context, pack_id)) return;
            bind_runtime<sprite::Runtime>(
                with<Runtimes>::modify(context, device)->sprites_id_mapping,
                pack_id,
                sprite::Pack::Actions::materialize(context, pack_id, device));
        }

    } // namespace

    auto Assets::Actions::singleton(Reading context) -> Id {
        return with<Assets>::get_global(context).singleton;
    }

    auto Assets::Actions::add_texture_loader(Writing context, Unit::Name name, texture::Loader::Quantum loader) -> texture::Asset::Id {
        return register_unit<texture::Asset, texture::Loader>(context, std::move(name), texture::Asset::Quantum{}, std::move(loader));
    }

    auto Assets::Actions::add_texture_generator(Writing context, Unit::Name name, texture::Generator::Quantum generator) -> texture::Asset::Id {
        return register_unit<texture::Asset, texture::Generator>(context, std::move(name), texture::Asset::Quantum{}, std::move(generator));
    }

    auto Assets::Actions::add_texpack_catalog(Writing context, Unit::Name name, texpack::LoaderCatalog::Quantum loader, index2 layerSize, integer capacity) -> texpack::Pack::Id {
        return register_unit<texpack::Pack, texpack::LoaderCatalog>(
            context,
            std::move(name),
            texpack::Pack::Quantum{
                .layerSize = layerSize,
                .capacity = capacity,
                .layers = {},
            },
            std::move(loader));
    }

    auto Assets::Actions::add_shader_loader(Writing context, Unit::Name name, shader::Loader::Quantum loader) -> shader::Asset::Id {
        return register_unit<shader::Asset, shader::Loader>(context, std::move(name), shader::Asset::Quantum{}, std::move(loader));
    }

    auto Assets::Actions::add_material(Writing context, Unit::Name name, material::Asset::Quantum asset) -> material::Asset::Id {
        const auto assets = singleton(context);
        const auto unit_id = with<Unit_group>::addElement(context, assets, Unit::Quantum{.name = std::move(name)});
        with<material::Asset>::extend(context, unit_id, std::move(asset));
        return unit_id;
    }

    auto Assets::Actions::add_overlay(Writing context, Unit::Name name, overlay::Asset::Quantum asset) -> overlay::Asset::Id {
        const auto assets = singleton(context);
        const auto unit_id = with<Unit_group>::addElement(context, assets, Unit::Quantum{.name = std::move(name)});
        with<overlay::Asset>::extend(context, unit_id, std::move(asset));
        return unit_id;
    }

    auto Assets::Actions::add_shadow_allocator(Writing context, Unit::Name name, shadow::Allocator::Quantum allocator) -> shadow::Asset::Id {
        return register_unit<shadow::Asset, shadow::Allocator>(context, std::move(name), shadow::Asset::Quantum{}, std::move(allocator));
    }

    auto Assets::Actions::add_geometry_loader(Writing context, Unit::Name name, geometry::Loader::Quantum loader) -> geometry::Asset::Id {
        return register_unit<geometry::Asset, geometry::Loader>(context, std::move(name), geometry::Asset::Quantum{
            .entries = {},
            .surfaces = {},
            .mounts = {},
            .entryCatalog = {},
            .surfaceCatalogs = {},
        }, std::move(loader));
    }

    auto Assets::Actions::add_geometry_generator(Writing context, Unit::Name name, geometry::Generator::Quantum generator) -> geometry::Asset::Id {
        return register_unit<geometry::Asset, geometry::Generator>(context, std::move(name), geometry::Asset::Quantum{
            .entries = {},
            .surfaces = {},
            .mounts = {},
            .entryCatalog = {},
            .surfaceCatalogs = {},
        }, std::move(generator));
    }

    auto Assets::Actions::add_sprites_kenney(Writing context, Unit::Name name, sprite::LoaderKenney::Quantum loader) -> sprite::Pack::Id {
        const auto texture_id = add_texture_loader(
            context,
            name,
            texture::Loader::Quantum{.file = loader.image, .mipmaps = false});

        return register_unit<sprite::Pack, sprite::LoaderKenney>(
            context,
            std::move(name),
            sprite::Pack::Quantum{
                .texture = with<Unit>::remember(context, texture_id),
                .entries = {},
            },
            std::move(loader));
    }

    auto Assets::Actions::add_meshpack_objs_loader(Writing context, Unit::Name name, meshpack::LoaderObjs::Quantum loader) -> meshpack::Asset::Id {
        return register_unit<meshpack::Asset, meshpack::LoaderObjs>(
            context,
            std::move(name),
            meshpack::Asset::Quantum{.texpack = {}, .entries = {}},
            std::move(loader));
    }

    auto Assets::Actions::add_meshpack_lwo_loader(Writing context, Unit::Name name, meshpack::LoaderLwo::Quantum loader) -> meshpack::Asset::Id {
        return register_unit<meshpack::Asset, meshpack::LoaderLwo>(
            context,
            std::move(name),
            meshpack::Asset::Quantum{.texpack = {}, .entries = {}},
            std::move(loader));
    }

    void Assets::Actions::extend(Writing context, filepath path) {
        const auto manager = with<Manager>::singleton(context);
        with<Manager>::modify(context, manager)->location = std::move(path);
        with<Assets>::modify_global(context)->singleton = manager;
    }

    void Runtimes::Actions::install(Writing context, Id device) {
        if (with<Runtimes>::exists(context, device)) return (void)context.refuse(std::format("resource::Runtimes::install: already installed for device {}", device));

        const auto assets = with<Assets>::singleton(context);
        with<DeviceRuntimes>::extend(context, device, DeviceRuntimes::Quantum{.assets = assets});
        with<Runtime_group>::extend(context, device);
        with<TexpackRuntime_group>::extend(context, device);
        with<Texture3arrayRuntime_group>::extend(context, device);
        with<ShaderRuntime_group>::extend(context, device);
        with<MaterialRuntime_group>::extend(context, device);
        with<OverlayRuntime_group>::extend(context, device);
        with<ShadowRuntime_group>::extend(context, device);
        with<GeometryRuntime_group>::extend(context, device);
        with<SpriteRuntime_group>::extend(context, device);
        BaseActions::extend(context, device, Quantum{});
    }

    void Runtimes::Actions::materialize(Writing context, Id device) {
        const auto assets = with<Assets>::singleton(context);
        with<DeviceRuntimes>::modify(context, device)->assets = assets;

        for (const auto [id, _] : context->aspect<texture::Asset>().items()) {
            rematerialize_texture(context, id, device);
        }
        for (const auto [id, _] : context->aspect<texpack::Pack>().items()) {
            rematerialize_texpack(context, id, device);
        }
        for (const auto [id, _] : context->aspect<shader::Asset>().items()) {
            rematerialize_shader(context, id, device);
        }
        for (const auto [id, _] : context->aspect<material::Asset>().items()) {
            rematerialize_material(context, id, device);
        }
        for (const auto [id, _] : context->aspect<overlay::Asset>().items()) {
            rematerialize_overlay(context, id, device);
        }
        for (const auto [id, _] : context->aspect<shadow::Asset>().items()) {
            rematerialize_shadow(context, id, device);
        }
        for (const auto [id, _] : context->aspect<geometry::Asset>().items()) {
            rematerialize_geometry(context, id, device);
        }
        for (const auto [id, _] : context->aspect<meshpack::LoaderObjs>().items()) {
            meshpack::LoaderObjs::Actions::finalize(context, id);
        }
        for (const auto [id, _] : context->aspect<meshpack::LoaderLwo>().items()) {
            meshpack::LoaderLwo::Actions::finalize(context, id);
        }
        for (const auto [id, _] : context->aspect<sprite::Pack>().items()) {
            rematerialize_sprites(context, id, device);
        }
    }

    struct Runtimes::Internals : Runtimes::DefaultInternals {
        static void maintain_all_mappings(Reacting context) {
            for (const auto [runtimes_id, quantum] : context.proposal.aspect<Runtimes>().items()) {
                scrub_mapping<texture::Asset, texture::Runtime>(context, runtimes_id, quantum.textures_id_mapping, &Quantum::textures_id_mapping);
                scrub_mapping<texpack::Pack, texpack::Runtime>(context, runtimes_id, quantum.texpacks_id_mapping, &Quantum::texpacks_id_mapping);
                scrub_mapping<texture3array::Asset, texture3array::Runtime>(context, runtimes_id, quantum.texture3arrays_id_mapping, &Quantum::texture3arrays_id_mapping);
                scrub_mapping<shader::Asset, shader::Runtime>(context, runtimes_id, quantum.shaders_id_mapping, &Quantum::shaders_id_mapping);
                scrub_mapping<material::Asset, material::Runtime>(context, runtimes_id, quantum.materials_id_mapping, &Quantum::materials_id_mapping);
                scrub_mapping<overlay::Asset, overlay::Runtime>(context, runtimes_id, quantum.overlays_id_mapping, &Quantum::overlays_id_mapping);
                scrub_mapping<shadow::Asset, shadow::Runtime>(context, runtimes_id, quantum.shadows_id_mapping, &Quantum::shadows_id_mapping);
                scrub_mapping<geometry::Asset, geometry::Runtime>(context, runtimes_id, quantum.geometries_id_mapping, &Quantum::geometries_id_mapping);
                scrub_mapping<sprite::Pack, sprite::Runtime>(context, runtimes_id, quantum.sprites_id_mapping, &Quantum::sprites_id_mapping);
            }
        }
    };

    auto Runtimes::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Runtimes, Assets>(&Runtimes::Internals::maintain_all_mappings),
        };
    }

}
