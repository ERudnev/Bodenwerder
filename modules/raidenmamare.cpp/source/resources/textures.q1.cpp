#include <rmmr/resources/textures.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <base/logging.h>

#include <filesystem>

namespace rmmr::resource::texture {

    using namespace fqsm::api;

    namespace {

        auto resolve_under_manager(const Manager::Quantum& manager, const Unit::Quantum& unit, const filename& relative) -> filepath {
            const std::filesystem::path file_path(relative);
            if (file_path.is_absolute()) {
                return file_path;
            }
            if (unit.library.empty()) {
                return manager.location / file_path;
            }
            return manager.location / unit.library / file_path;
        }

    } // namespace

    auto Loader::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> Runtime::Quantum {
        const auto& loader = with<Loader>::get(context, asset_id);
        const auto& unit = with<Unit>::get(context, asset_id);
        const auto& manager = with<Manager>::get(context, unit.manager);

        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        const auto path = resolve_under_manager(manager, unit, loader.file);

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (not pixels) {
            return context.refuse("resource::texture::Loader::materialize: failed to load image: " + path.string());
        }

        Runtime::Handle handle{};
        glGenTextures(1, &handle);
        if (not handle) {
            stbi_image_free(pixels);
            return context.refuse("resource::texture::Loader::materialize: glGenTextures failed");
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

        return Runtime::Quantum{
            .device = device,
            .handle = handle,
            .size = index2{width, height},
        };
    }

    auto Generator::Actions::materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum {
        _INCOMPLETE_;
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id runtime_id, const Quantum& last) {
            if (not last.handle) {
                return;
            }

            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            auto handle = last.handle;
            glDeleteTextures(1, &handle);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
