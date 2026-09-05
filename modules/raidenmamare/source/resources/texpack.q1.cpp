#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <base/logging.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <format>
#include <vector>

namespace rmmr::resource::texpack {

    using namespace fqsm::api;

    namespace {

        auto is_image_extension(const std::filesystem::path& path) -> bool {
            auto ext = path.extension().string();
            for (char& ch : ext) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return ext == ".jpg" or ext == ".jpeg" or ext == ".jfif" or ext == ".png" or ext == ".bmp" or ext == ".tga" or ext == ".gif";
        }

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (not last.handle) {
                return;
            }
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            auto handle = last.handle;
            glDeleteTextures(1, &handle);
        }

        auto install_runtime(Writing context, system::Device::Id device, Pack::Id pack_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.texpacks_id_mapping.find(pack_id); existing != runtimes.texpacks_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<TexpackRuntime_group>::addElement(context, device, std::move(quantum));
        }

        void resample_rgba(
            const unsigned char* src,
            int src_w,
            int src_h,
            unsigned char* dst,
            int dst_w,
            int dst_h)
        {
            for (int y = 0; y < dst_h; ++y) {
                const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(dst_h);
                const float sy = v * static_cast<float>(src_h) - 0.5f;
                const int y0 = std::max(0, std::min(src_h - 1, static_cast<int>(std::floor(sy))));
                const int y1 = std::max(0, std::min(src_h - 1, y0 + 1));
                const float fy = sy - static_cast<float>(y0);
                for (int x = 0; x < dst_w; ++x) {
                    const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(dst_w);
                    const float sx = u * static_cast<float>(src_w) - 0.5f;
                    const int x0 = std::max(0, std::min(src_w - 1, static_cast<int>(std::floor(sx))));
                    const int x1 = std::max(0, std::min(src_w - 1, x0 + 1));
                    const float fx = sx - static_cast<float>(x0);
                    for (int c = 0; c < 4; ++c) {
                        const auto sample = [&](int px, int py) -> float {
                            return static_cast<float>(src[(static_cast<std::size_t>(py) * static_cast<std::size_t>(src_w) + static_cast<std::size_t>(px)) * 4u + static_cast<std::size_t>(c)]);
                        };
                        const float v00 = sample(x0, y0);
                        const float v10 = sample(x1, y0);
                        const float v01 = sample(x0, y1);
                        const float v11 = sample(x1, y1);
                        const float top = v00 * (1.0f - fx) + v10 * fx;
                        const float bottom = v01 * (1.0f - fx) + v11 * fx;
                        dst[(static_cast<std::size_t>(y) * static_cast<std::size_t>(dst_w) + static_cast<std::size_t>(x)) * 4u + static_cast<std::size_t>(c)] =
                            static_cast<unsigned char>(top * (1.0f - fy) + bottom * fy + 0.5f);
                    }
                }
            }
        }

    } // namespace

    void LoaderCatalog::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderCatalog>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto dir_path = with<Manager>::resolve(context, unit, loader.directory);
        base::whisper("rmmr: texpack::LoaderCatalog '{}' ← {}", unit.name.text(), dir_path.string());

        if (not std::filesystem::is_directory(dir_path)) {
            return (void)context.refuse(std::format(
                "resource::texpack::LoaderCatalog::load: '{}' is not a directory",
                dir_path.string()));
        }

        const auto basename = dir_path.filename().string();
        if (basename != unit.name.own) {
            return (void)context.refuse(std::format(
                "resource::texpack::LoaderCatalog::load: directory basename '{}' != pack own name '{}'",
                basename,
                unit.name.own));
        }

        vector<string> layers{};
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (not entry.is_regular_file()) {
                continue;
            }
            if (not is_image_extension(entry.path())) {
                continue;
            }
            layers.push_back(entry.path().filename().string());
        }
        std::sort(layers.begin(), layers.end());
        if (layers.empty()) {
            return (void)context.refuse(std::format(
                "resource::texpack::LoaderCatalog::load: '{}' has no image files",
                dir_path.string()));
        }

        auto pack = with<Pack>::modify(context, pack_id);
        if (static_cast<integer>(layers.size()) > pack->capacity) {
            return (void)context.refuse(std::format(
                "resource::texpack::LoaderCatalog::load: '{}' layers ({}) exceed capacity ({})",
                unit.name.text(),
                layers.size(),
                pack->capacity));
        }
        pack->layers = std::move(layers);
        base::message("rmmr: texpack '{}' loaded ({} layers from '{}')", unit.name.text(), with<Pack>::get(context, pack_id).layers.size(), dir_path.string());
    }

    auto Pack::Actions::materialize(Writing context, Id pack_id, system::Device::Id device) -> optional<Runtime::Id> {
        if (not with<LoaderCatalog>::exists(context, pack_id)) {
            return context.refuse("resource::texpack::Pack::materialize: LoaderCatalog missing");
        }
        const auto& pack = with<Pack>::get(context, pack_id);
        const auto& loader = with<LoaderCatalog>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        if (pack.layers.empty()) {
            return context.refuse(std::format(
                "resource::texpack::Pack::materialize: '{}' layers empty (LoaderCatalog::load did not run)",
                unit.name.text()));
        }

        const int layer_w = static_cast<int>(pack.layerSize.x);
        const int layer_h = static_cast<int>(pack.layerSize.y);
        if (layer_w <= 0 || layer_h <= 0) {
            return context.refuse("resource::texpack::Pack::materialize: layerSize must be positive");
        }
        if (pack.capacity <= 0) {
            return context.refuse("resource::texpack::Pack::materialize: capacity must be positive");
        }
        if (static_cast<integer>(pack.layers.size()) > pack.capacity) {
            return context.refuse(std::format(
                "resource::texpack::Pack::materialize: '{}' layers ({}) exceed capacity ({})",
                unit.name.text(),
                pack.layers.size(),
                pack.capacity));
        }

        const auto dir_path = with<Manager>::resolve(context, unit, loader.directory);
        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        renderer::Texture handle{};
        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &handle);
        if (not handle) {
            return context.refuse("resource::texpack::Pack::materialize: glCreateTextures failed");
        }

        const int levels = 1 + static_cast<int>(std::floor(std::log2(std::max(layer_w, layer_h))));
        glTextureStorage3D(handle, levels, GL_RGBA8, layer_w, layer_h, static_cast<GLsizei>(pack.capacity));
        glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        umap<string, integer> layer_map{};
        vector<unsigned char> dst(static_cast<std::size_t>(layer_w) * static_cast<std::size_t>(layer_h) * 4u);
        integer layer_index = 0;
        for (const auto& layer_name : pack.layers) {
            const auto file_path = dir_path / layer_name;
            int src_w = 0;
            int src_h = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load(file_path.string().c_str(), &src_w, &src_h, &channels, STBI_rgb_alpha);
            if (not pixels) {
                glDeleteTextures(1, &handle);
                return context.refuse(std::format(
                    "resource::texpack::Pack::materialize: failed to load '{}'",
                    file_path.string()));
            }
            resample_rgba(pixels, src_w, src_h, dst.data(), layer_w, layer_h);
            stbi_image_free(pixels);
            glTextureSubImage3D(handle, 0, 0, 0, static_cast<GLint>(layer_index), layer_w, layer_h, 1, GL_RGBA, GL_UNSIGNED_BYTE, dst.data());
            layer_map.emplace(layer_name, layer_index);
            ++layer_index;
        }
        glGenerateTextureMipmap(handle);

        base::message("rmmr: texpack '{}' materialize {} layers ({}x{}, capacity {})", unit.name.text(), layer_index, layer_w, layer_h, pack.capacity);
        return install_runtime(context, device, pack_id, Runtime::Quantum{
            .device = device,
            .handle = handle,
            .layerSize = pack.layerSize,
            .capacity = pack.capacity,
            .layers = std::move(layer_map),
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
