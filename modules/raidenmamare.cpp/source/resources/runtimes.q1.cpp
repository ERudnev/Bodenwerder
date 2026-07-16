#include <rmmr/resources/runtimes.q1.h>

#include <format>
#include <stdexcept>

namespace rmmr::resource {

    using namespace fqsm::api;

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

    void Assets::Actions::extend(Writing context, Manager::Id manager, filepath path) {
        if (not with<Manager>::exists(context, manager)) {
            throw std::runtime_error("resource::Assets::extend: manager does not exist");
        }

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
        if (with<Runtimes>::exists(context, device)) {
            context.refuse(std::format("resource::Runtimes::install: already installed for device {}", device));
            return;
        }

        with<DeviceRuntimes>::extend(context, device, DeviceRuntimes::Quantum{
            .assets = with<system::Device>::get(context, device).core,
        });
        with<Runtime_group>::extend(context, device);
        with<ShaderRuntime_group>::extend(context, device);
        BaseActions::extend(context, device, Quantum{});
    }

    void Runtimes::Actions::materialize(Writing context, Id device, Assets::Id assets) {
        if (not with<DeviceRuntimes>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: DeviceRuntimes missing for device");
        }
        with<DeviceRuntimes>::modify(context, device)->assets = assets;

        if (not with<Runtimes>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: Runtimes missing for device");
        }
        auto runtimes = with<Runtimes>::modify(context, device);

        if (not with<Runtime_group>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: Runtime_group missing for device");
        }
        if (not with<ShaderRuntime_group>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: ShaderRuntime_group missing for device");
        }

        const auto rebuild_texture_runtime = [&](texture::Asset::Id asset_id, texture::Runtime::Quantum runtime) {
            if (const auto existing = runtimes->textures_id_mapping.find(asset_id); existing != runtimes->textures_id_mapping.end()) {
                with<texture::Runtime>::remove(context, existing->second);
                runtimes->textures_id_mapping.erase(asset_id);
            }

            const auto runtime_id = with<Runtime_group>::addElement(context, device, std::move(runtime));
            runtimes->textures_id_mapping.emplace(asset_id, runtime_id);
        };
        const auto rebuild_shader_runtime = [&](shader::Asset::Id asset_id, shader::Runtime::Quantum runtime) {
            if (const auto existing = runtimes->shaders_id_mapping.find(asset_id); existing != runtimes->shaders_id_mapping.end()) {
                with<shader::Runtime>::remove(context, existing->second);
                runtimes->shaders_id_mapping.erase(asset_id);
            }

            const auto runtime_id = with<ShaderRuntime_group>::addElement(context, device, std::move(runtime));
            runtimes->shaders_id_mapping.emplace(asset_id, runtime_id);
        };

        for (const auto entry : context->aspect<texture::FromFile>().items()) {
            const auto unit_id = entry.id;
            if (with<Unit>::get(context, unit_id).manager != assets) {
                continue;
            }

            rebuild_texture_runtime(unit_id, texture::FromFile::Actions::materialize(context, unit_id, device));
        }

        for (const auto entry : context->aspect<texture::Generated>().items()) {
            const auto unit_id = entry.id;
            if (with<Unit>::get(context, unit_id).manager != assets) {
                continue;
            }

            rebuild_texture_runtime(unit_id, texture::Generated::Actions::materialize(context, unit_id, device));
        }

        for (const auto entry : context->aspect<shader::FromFile>().items()) {
            const auto unit_id = entry.id;
            if (with<Unit>::get(context, unit_id).manager != assets) {
                continue;
            }

            const auto runtime = shader::FromFile::Actions::materialize(context, unit_id, device);
            if (runtime.handle) {
                rebuild_shader_runtime(unit_id, runtime);
            }
        }
    }

    struct Runtimes::Internals : Runtimes::DefaultInternals {
        static void maintain_all_mappings(Reacting context) {
            auto& texture_runtime_patch = context.reaction<texture::Runtime>();
            auto& shader_runtime_patch = context.reaction<shader::Runtime>();
            auto& runtimes_patch = context.reaction<Runtimes>();

            for (const auto entry : context.proposal.aspect<Runtimes>().items()) {
                for (const auto& [asset_id, runtime_id] : entry.value.textures_id_mapping) {
                    const bool asset_exists = with<texture::Asset>::exists(context, asset_id);
                    const bool runtime_exists = with<texture::Runtime>::exists(context, runtime_id);

                    if (asset_exists && runtime_exists) {
                        continue;
                    }

                    if (runtime_exists) {
                        texture_runtime_patch.put_deletion(runtime_id);
                    }

                    auto& fixed = runtimes_patch.update_modification(entry.id, [&]() -> const Quantum& {
                        return with<Runtimes>::get(context, entry.id);
                    });
                    fixed.textures_id_mapping.erase(asset_id);
                }

                for (const auto& [asset_id, runtime_id] : entry.value.shaders_id_mapping) {
                    const bool asset_exists = with<shader::Asset>::exists(context, asset_id);
                    const bool runtime_exists = with<shader::Runtime>::exists(context, runtime_id);

                    if (asset_exists && runtime_exists) {
                        continue;
                    }

                    if (runtime_exists) {
                        shader_runtime_patch.put_deletion(runtime_id);
                    }

                    auto& fixed = runtimes_patch.update_modification(entry.id, [&]() -> const Quantum& {
                        return with<Runtimes>::get(context, entry.id);
                    });
                    fixed.shaders_id_mapping.erase(asset_id);
                }
            }
        }
    };

    auto Runtimes::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Runtimes, Assets>(&Runtimes::Internals::maintain_all_mappings),
        };
    }

}
