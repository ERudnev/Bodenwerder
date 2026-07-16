#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <format>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace rmmr::resource {

    using namespace fqsm::api;

    namespace {

        auto resolve_under_manager(
            const Manager::Quantum& manager,
            const Unit::Quantum& unit,
            const filename& relative
        ) -> filepath {
            const std::filesystem::path file_path(relative);
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

            const auto path = resolve_under_manager(manager, unit, from_file.file);

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

        auto read_text_file(const std::filesystem::path& path) -> maybe<std::string> {
            std::ifstream input(path, std::ios::binary);
            if (not input) {
                return {};
            }
            return std::string{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>(),
            };
        }

        auto compile_shader_stage(GLenum shader_type, const std::string& source) -> maybe<GLuint> {
            const GLuint shader = glCreateShader(shader_type);
            const char* source_ptr = source.c_str();
            glShaderSource(shader, 1, &source_ptr, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success) {
                return shader;
            }

            glDeleteShader(shader);
            return {};
        }

        auto materialize_file_shader(
            Writing context,
            system::Device::Id device,
            const Manager::Quantum& manager,
            const Unit::Quantum& unit,
            const shader::FromFile::Quantum& from_file
        ) -> maybe<shader::Runtime::Quantum> {
            const auto& device_quantum = with<system::Device>::get(context, device);
            glfwMakeContextCurrent(device_quantum.handle);

            const auto vertex_path = resolve_under_manager(manager, unit, from_file.vertex);
            const auto fragment_path = resolve_under_manager(manager, unit, from_file.fragment);

            const auto vertex_source = read_text_file(vertex_path);
            if (not vertex_source or vertex_source->empty()) {
                context.deny("resource::Runtimes::materialize: vertex shader unreadable: " + vertex_path.string());
                return {};
            }

            const auto fragment_source = read_text_file(fragment_path);
            if (not fragment_source or fragment_source->empty()) {
                context.deny("resource::Runtimes::materialize: fragment shader unreadable: " + fragment_path.string());
                return {};
            }

            const auto vertex_shader = compile_shader_stage(GL_VERTEX_SHADER, *vertex_source);
            if (not vertex_shader) {
                context.deny("resource::Runtimes::materialize: vertex shader compile failed: " + std::string(from_file.vertex));
                return {};
            }

            const auto fragment_shader = compile_shader_stage(GL_FRAGMENT_SHADER, *fragment_source);
            if (not fragment_shader) {
                glDeleteShader(*vertex_shader);
                context.deny("resource::Runtimes::materialize: fragment shader compile failed: " + std::string(from_file.fragment));
                return {};
            }

            const GLuint program = glCreateProgram();
            if (not program) {
                glDeleteShader(*vertex_shader);
                glDeleteShader(*fragment_shader);
                context.deny("resource::Runtimes::materialize: glCreateProgram failed");
                return {};
            }

            glAttachShader(program, *vertex_shader);
            glAttachShader(program, *fragment_shader);
            glLinkProgram(program);

            int link_ok = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
            glDeleteShader(*vertex_shader);
            glDeleteShader(*fragment_shader);

            if (not link_ok) {
                char info_log[2048];
                glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
                glDeleteProgram(program);
                context.deny(std::string("resource::Runtimes::materialize: program link failed: ") + info_log);
                return {};
            }

            return shader::Runtime::Quantum{
                .device = device,
                .handle = program,
            };
        }

    } // namespace

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

    auto Assets::Actions::add_shader_file(
        Writing context,
        Id assets,
        Unit::Quantum unit,
        shader::Asset::Quantum asset,
        shader::FromFile::Quantum from_file
    ) -> shader::Asset::Id {
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
            context.deny(std::format("resource::Runtimes::install: already installed for device {}", device));
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

        const auto& manager = with<Manager>::get(context, assets);
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
            const auto& unit = with<Unit>::get(context, unit_id);
            if (unit.manager != assets) {
                continue;
            }

            rebuild_texture_runtime(unit_id, materialize_file_texture(context, device, manager, unit, entry.value));
        }

        for (const auto entry : context->aspect<texture::Generated>().items()) {
            const auto unit_id = entry.id;
            const auto& unit = with<Unit>::get(context, unit_id);
            if (unit.manager != assets) {
                continue;
            }

            _INCOMPLETE_;
        }

        for (const auto entry : context->aspect<shader::FromFile>().items()) {
            const auto unit_id = entry.id;
            const auto& unit = with<Unit>::get(context, unit_id);
            if (unit.manager != assets) {
                continue;
            }

            if (auto runtime = materialize_file_shader(context, device, manager, unit, entry.value)) {
                rebuild_shader_runtime(unit_id, *runtime);
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
