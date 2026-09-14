#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <base/logging.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
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

        auto rgb565(vec3 color) -> std::uint16_t {
            const int red = static_cast<int>(color.r * 31.0f + 0.5f);
            const int green = static_cast<int>(color.g * 63.0f + 0.5f);
            const int blue = static_cast<int>(color.b * 31.0f + 0.5f);
            return static_cast<std::uint16_t>((red << 11) | (green << 5) | blue);
        }

        auto rgb565ToVec3(std::uint16_t packed) -> vec3 {
            const float red = static_cast<float>((packed >> 11) & 31) / 31.0f;
            const float green = static_cast<float>((packed >> 5) & 63) / 63.0f;
            const float blue = static_cast<float>(packed & 31) / 31.0f;
            return vec3{red, green, blue};
        }

        void compressBc1Block(const unsigned char* rgba, int stride, unsigned char* out) {
            vec3 colors[16];
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    const unsigned char* pixel = rgba + static_cast<std::size_t>(row) * static_cast<std::size_t>(stride) * 4u + static_cast<std::size_t>(col) * 4u;
                    colors[row * 4 + col] = vec3{static_cast<float>(pixel[0]) / 255.0f, static_cast<float>(pixel[1]) / 255.0f, static_cast<float>(pixel[2]) / 255.0f};
                }
            }
            vec3 minColor = colors[0];
            vec3 maxColor = colors[0];
            for (int index = 1; index < 16; ++index) {
                minColor = glm::min(minColor, colors[index]);
                maxColor = glm::max(maxColor, colors[index]);
            }
            const std::uint16_t c0 = rgb565(maxColor);
            const std::uint16_t c1 = rgb565(minColor);
            out[0] = static_cast<unsigned char>(c0 & 255u);
            out[1] = static_cast<unsigned char>(c0 >> 8);
            out[2] = static_cast<unsigned char>(c1 & 255u);
            out[3] = static_cast<unsigned char>(c1 >> 8);
            vec3 palette[4];
            palette[0] = rgb565ToVec3(c0);
            palette[1] = rgb565ToVec3(c1);
            palette[2] = palette[0] * (2.0f / 3.0f) + palette[1] * (1.0f / 3.0f);
            palette[3] = palette[0] * (1.0f / 3.0f) + palette[1] * (2.0f / 3.0f);
            std::uint32_t indices = 0;
            for (int index = 0; index < 16; ++index) {
                float best = glm::length(colors[index] - palette[0]);
                int pick = 0;
                for (int candidate = 1; candidate < 4; ++candidate) {
                    const float distance = glm::length(colors[index] - palette[candidate]);
                    if (distance < best) {
                        best = distance;
                        pick = candidate;
                    }
                }
                indices |= static_cast<std::uint32_t>(pick) << (index * 2);
            }
            out[4] = static_cast<unsigned char>(indices & 255u);
            out[5] = static_cast<unsigned char>((indices >> 8) & 255u);
            out[6] = static_cast<unsigned char>((indices >> 16) & 255u);
            out[7] = static_cast<unsigned char>((indices >> 24) & 255u);
        }

        void compressBc4Block(const unsigned char* rgba, int stride, unsigned char* out, int channel) {
            float values[16];
            float minValue = 1.0f;
            float maxValue = 0.0f;
            for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                    const unsigned char* pixel = rgba + static_cast<std::size_t>(row) * static_cast<std::size_t>(stride) * 4u + static_cast<std::size_t>(col) * 4u;
                    const float value = static_cast<float>(pixel[channel]) / 255.0f;
                    values[row * 4 + col] = value;
                    minValue = std::min(minValue, value);
                    maxValue = std::max(maxValue, value);
                }
            }
            const auto end0 = static_cast<unsigned char>(maxValue * 255.0f + 0.5f);
            const auto end1 = static_cast<unsigned char>(minValue * 255.0f + 0.5f);
            out[0] = end0;
            out[1] = end1;
            float palette[8];
            palette[0] = static_cast<float>(end0) / 255.0f;
            palette[1] = static_cast<float>(end1) / 255.0f;
            if (end0 > end1) {
                for (int step = 2; step < 8; ++step)
                    palette[step] = palette[0] + (palette[1] - palette[0]) * (static_cast<float>(step - 1) / 7.0f);
            } else {
                palette[2] = (palette[0] + palette[1]) * 0.5f;
                palette[3] = palette[2];
                palette[4] = 0.0f;
                palette[5] = 1.0f;
                palette[6] = 0.0f;
                palette[7] = 1.0f;
            }
            std::uint64_t indices = 0;
            for (int index = 0; index < 16; ++index) {
                float best = std::abs(values[index] - palette[0]);
                int pick = 0;
                for (int candidate = 1; candidate < 8; ++candidate) {
                    const float distance = std::abs(values[index] - palette[candidate]);
                    if (distance < best) {
                        best = distance;
                        pick = candidate;
                    }
                }
                indices |= static_cast<std::uint64_t>(pick) << (index * 3);
            }
            for (int byte = 0; byte < 6; ++byte)
                out[2 + byte] = static_cast<unsigned char>((indices >> (byte * 8)) & 255u);
        }

        auto compressRgba(const unsigned char* rgba, int width, int height, bool grayscale) -> vector<unsigned char> {
            const int blocksX = width / 4;
            const int blocksY = height / 4;
            const std::size_t blockBytes = grayscale ? 8u : 16u;
            vector<unsigned char> out(static_cast<std::size_t>(blocksX * blocksY) * blockBytes);
            vector<unsigned char> block(blockBytes);
            for (int blockY = 0; blockY < blocksY; ++blockY) {
                for (int blockX = 0; blockX < blocksX; ++blockX) {
                    const unsigned char* source = rgba + (static_cast<std::size_t>(blockY * 4) * static_cast<std::size_t>(width) + static_cast<std::size_t>(blockX * 4)) * 4u;
                    if (grayscale)
                        compressBc4Block(source, width, block.data(), 0);
                    else {
                        compressBc4Block(source, width, block.data(), 3);
                        compressBc1Block(source, width, block.data() + 8);
                    }
                    const std::size_t offset = (static_cast<std::size_t>(blockY * blocksX + blockX)) * blockBytes;
                    for (std::size_t byte = 0; byte < blockBytes; ++byte)
                        out[offset + byte] = block[byte];
                }
            }
            return out;
        }

        auto compressedFormat(bool grayscale) -> GLenum {
            if (grayscale)
                return GL_COMPRESSED_RED_RGTC1;
            if (GLEW_EXT_texture_compression_s3tc)
                return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            return GL_RGBA8;
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

        const bool useCompression = pack.compressed and (layer_w % 4 == 0) and (layer_h % 4 == 0) and (pack.grayscale or GLEW_EXT_texture_compression_s3tc);
        const GLenum internalFormat = useCompression ? compressedFormat(pack.grayscale) : GL_RGBA8;
        if (pack.compressed and not useCompression)
            base::warning("rmmr: texpack '{}' compression unavailable, storing RGBA8", unit.name.text());
        const int levels = 1 + static_cast<int>(std::floor(std::log2(std::max(layer_w, layer_h))));
        glTextureStorage3D(handle, levels, internalFormat, layer_w, layer_h, static_cast<GLsizei>(pack.capacity));
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
            if (useCompression) {
                const auto blocks = compressRgba(dst.data(), layer_w, layer_h, pack.grayscale);
                glCompressedTextureSubImage3D(handle, 0, 0, 0, static_cast<GLint>(layer_index), layer_w, layer_h, 1, internalFormat, static_cast<GLsizei>(blocks.size()), blocks.data());
            } else {
                glTextureSubImage3D(handle, 0, 0, 0, static_cast<GLint>(layer_index), layer_w, layer_h, 1, GL_RGBA, GL_UNSIGNED_BYTE, dst.data());
            }
            layer_map.emplace(layer_name, layer_index);
            ++layer_index;
        }
        glGenerateTextureMipmap(handle);

        base::message("rmmr: texpack '{}' materialize {} layers ({}x{}, capacity {}, {})", unit.name.text(), layer_index, layer_w, layer_h, pack.capacity, useCompression ? (pack.grayscale ? "BC4" : "BC3") : "RGBA8");
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
