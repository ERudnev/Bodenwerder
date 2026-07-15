#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <base/logging.h>

#include <filesystem>
#include <stdexcept>

namespace rmmr::resource {

    using namespace fqsm::api;

    namespace {

        auto resolve_texture_path(
            const Manager::Quantum& manager,
            const Unit::Quantum& unit,
            const texture::FromFile::Quantum& from_file
        ) -> filepath {
            const std::filesystem::path file_path(from_file.file);
            if (file_path.is_absolute()) {
                return file_path;
            }
            if (unit.library.empty()) {
                return manager.location / file_path;
            }
            return manager.location / unit.library / file_path;
        }

        auto materialize_file_texture(
            Writing context,
            system::Device::Id device,
            const Manager::Quantum& manager,
            const Unit::Quantum& unit,
            const texture::FromFile::Quantum& from_file
        ) -> texture::Runtime::Quantum {
            const auto& device_quantum = with<system::Device>::get(context, device);
            glfwMakeContextCurrent(device_quantum.handle);

            const auto path = resolve_texture_path(manager, unit, from_file);

            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (not pixels) {
                throw std::runtime_error("resource::Runtimes::materialize: failed to load image: " + path.string());
            }

            texture::Runtime::Handle handle{};
            glGenTextures(1, &handle);
            if (not handle) {
                stbi_image_free(pixels);
                throw std::runtime_error("resource::Runtimes::materialize: glGenTextures failed");
            }

            glBindTexture(GL_TEXTURE_2D, handle);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            glGenerateMipmap(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, 0);

            stbi_image_free(pixels);

            return texture::Runtime::Quantum{
                .device = device,
                .handle = handle,
                .size = index2{width, height},
            };
        }

    } // namespace

    struct MaterializeRequest::Internals : MaterializeRequest::DefaultInternals {};

    auto MaterializeRequest::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::anchored<MaterializeRequest, Unit, &MaterializeRequest::Quantum::asset>{},
        };
    }

    auto Assets::Actions::add_texture_file(
        Writing context,
        Id assets,
        Unit::Quantum unit,
        texture::Asset::Quantum asset,
        texture::FromFile::Quantum from_file
    ) -> texture::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<texture::Asset>::extend(context, unit_id, std::move(asset));
        with<texture::FromFile>::extend(context, unit_id, std::move(from_file));
        return unit_id;
    }

    auto Assets::Actions::add_texture_generated(
        Writing context,
        Id assets,
        Unit::Quantum unit,
        texture::Asset::Quantum asset,
        texture::Generated::Quantum generated
    ) -> texture::Asset::Id {
        unit.manager = assets;

        if (not with<Unit_group>::exists(context, assets)) {
            with<Unit_group>::extend(context, assets);
        }

        const auto unit_id = with<Unit_group>::addElement(context, assets, std::move(unit));
        with<texture::Asset>::extend(context, unit_id, std::move(asset));
        with<texture::Generated>::extend(context, unit_id, std::move(generated));
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
            base::message("resource::Runtimes::install: already installed for device {}", device);
            return;
        }

        with<DeviceRuntimes>::extend(context, device, DeviceRuntimes::Quantum{
            .manager = with<system::Device>::get(context, device).core,
        });
        with<Runtime_group>::extend(context, device);
        BaseActions::extend(context, device, Quantum{});
    }

    void Runtimes::Actions::materialize(Writing context, Id device, Assets::Id assets) {
        if (not with<DeviceRuntimes>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: DeviceRuntimes missing for device");
        }
        with<DeviceRuntimes>::modify(context, device)->manager = assets;

        if (not with<Runtimes>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: Runtimes missing for device");
        }
        auto runtimes = with<Runtimes>::modify(context, device);

        if (not with<Runtime_group>::exists(context, device)) {
            throw std::runtime_error("resource::Runtimes::materialize: Runtime_group missing for device");
        }
        if (not with<Unit_group>::exists(context, assets)) {
            return;
        }

        const auto& manager = with<Manager>::get(context, assets);
        const auto& units = with<Unit_group>::get(context, assets);
        for (const auto unit_id : units) {
            if (not with<texture::Asset>::exists(context, unit_id)) {
                continue;
            }
            if (runtimes->textures_id_mapping.contains(unit_id)) {
                continue;
            }

            const auto& unit = with<Unit>::get(context, unit_id);
            if (with<texture::FromFile>::exists(context, unit_id)) {
                const auto& from_file = with<texture::FromFile>::get(context, unit_id);
                const auto runtime = materialize_file_texture(context, device, manager, unit, from_file);
                const auto runtime_id = with<Runtime_group>::addElement(context, device, runtime);
                runtimes->textures_id_mapping.emplace(unit_id, runtime_id);
                continue;
            }

            if (with<texture::Generated>::exists(context, unit_id)) {
                _INCOMPLETE_;
            }
        }
    }

    struct Runtimes::Internals : Runtimes::DefaultInternals {
        static void maintain_all_mappings(Reacting context) {
            auto& runtime_patch = context.reaction<texture::Runtime>();
            auto& runtimes_patch = context.reaction<Runtimes>();

            for (const auto entry : context.proposal.aspect<Runtimes>().items()) {
                bool touched = false;

                for (const auto& [asset_id, runtime_id] : entry.value.textures_id_mapping) {
                    const bool asset_exists = with<texture::Asset>::exists(context, asset_id);
                    const bool runtime_exists = with<texture::Runtime>::exists(context, runtime_id);

                    if (asset_exists && runtime_exists) {
                        continue;
                    }

                    if (runtime_exists) {
                        runtime_patch.put_deletion(runtime_id);
                    }

                    auto& fixed = runtimes_patch.update_modification(entry.id, [&]() -> const Quantum& {
                        return with<Runtimes>::get(context, entry.id);
                    });
                    fixed.textures_id_mapping.erase(asset_id);
                    touched = true;
                }

                (void)touched;
            }
        }
    };

    auto Runtimes::customAspectReactions() -> const Behavior {
        return {
            reaction::aspect_wide<Runtimes, Assets>(&Runtimes::Internals::maintain_all_mappings),
        };
    }

}
