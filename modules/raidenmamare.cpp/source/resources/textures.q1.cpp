#include <rmmr/resources/textures.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <cmath>
#include <filesystem>
#include <vector>

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

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (not last.handle) {
                return;
            }
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            auto handle = last.handle;
            glDeleteTextures(1, &handle);
        }

        auto install_runtime(Writing context, system::Device::Id device, Asset::Id asset_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.textures_id_mapping.find(asset_id); existing != runtimes.textures_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<Runtime_group>::addElement(context, device, std::move(quantum));
        }

    } // namespace

    auto Loader::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
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

        renderer::Texture handle{};
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

        return install_runtime(context, device, asset_id, Runtime::Quantum{
            .device = device,
            .handle = handle,
            .size = index2{width, height},
        });
    }

    auto Generator::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& generator = with<Generator>::get(context, asset_id);
        const int width = static_cast<int>(generator.size.x);
        const int height = static_cast<int>(generator.size.y);
        if (width <= 0 || height <= 0) {
            return context.refuse("resource::texture::Generator::materialize: size must be positive");
        }

        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        constexpr float outer_radius = 1.0f;
        constexpr float inner_radius = 0.82f;
        const bool invert_alpha = generator.pattern == Generator::Pattern::whiteRing;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width) * 2.0f - 1.0f;
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height) * 2.0f - 1.0f;
                const float radius = std::sqrt(u * u + v * v);
                float alpha = (radius - outer_radius) / (inner_radius - outer_radius);
                if (alpha < 0.0f) {
                    alpha = 0.0f;
                } else if (alpha > 1.0f) {
                    alpha = 1.0f;
                }
                alpha = alpha * alpha * (3.0f - 2.0f * alpha);
                if (invert_alpha) {
                    alpha = 1.0f - alpha;
                }

                const std::size_t pixel = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;
                pixels[pixel + 0] = 255;
                pixels[pixel + 1] = 255;
                pixels[pixel + 2] = 255;
                pixels[pixel + 3] = static_cast<unsigned char>(alpha * 255.0f + 0.5f);
            }
        }

        renderer::Texture handle{};
        glGenTextures(1, &handle);
        if (not handle) {
            return context.refuse("resource::texture::Generator::materialize: glGenTextures failed");
        }

        glBindTexture(GL_TEXTURE_2D, handle);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        return install_runtime(context, device, asset_id, Runtime::Quantum{
            .device = device,
            .handle = handle,
            .size = generator.size,
        });
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            release_gl(context, last);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
