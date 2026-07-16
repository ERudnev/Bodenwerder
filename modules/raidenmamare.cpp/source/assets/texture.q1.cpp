#include <rmmr/assets/texture.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <filesystem>
#include <string_view>

namespace rmmr::asset {

    using namespace fqsm::api;

    namespace {

        auto resolve_under_asset_root(const std::filesystem::path& asset_root, std::string_view relative_path) -> std::filesystem::path {
            const std::filesystem::path path(relative_path);
            if (path.is_absolute()) return path;
            return asset_root / path;
        }

    } // namespace

    auto Texture::Always::filename(const string& name, const string& library) -> string {
        if (library.empty()) {
            return "textures/" + name;
        }
        return library + "/textures/" + name;
    }

    auto Texture::Actions::compile(Writing context, Id asset_texture, system::Device::Id device) -> resource_old::Texture::Id {
        const auto& asset = with<Texture>::get(context, asset_texture);
        const auto& device_quantum = with<system::Device>::get(context, device);
        const auto& core_quantum = with<system::Core>::get(context, device_quantum.core);

        glfwMakeContextCurrent(device_quantum.handle);

        const auto& asset_root = core_quantum.assets_root;
        const auto path = resolve_under_asset_root(asset_root, Texture::Always::filename(asset.name, asset.library));

        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (not pixels) {
            return context.refuse("asset::Texture::compile: failed to load image: " + path.string());
        }

        resource_old::Texture::Handle handle{};
        glGenTextures(1, &handle);
        if (not handle) {
            stbi_image_free(pixels);
            return context.refuse("asset::Texture::compile: glGenTextures failed");
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

        with<resource_old::Texture_group>::extend(context, device);
        return with<resource_old::Texture_group>::addElement(context, device, resource_old::Texture::Quantum{
            .handle = handle,
            .size = index2{width, height},
        });
    }

}
