#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <cmath>
#include <algorithm>
#include <vector>

namespace rmmr::resource::texture3array {

    using namespace fqsm::api;

    namespace {

        void deleteHandles(const vector<renderer::Texture>& handles) {
            for (auto handle : handles) {
                if (handle)
                    glDeleteTextures(1, &handle);
            }
        }

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (last.layers.empty())
                return;
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            deleteHandles(last.layers);
        }

        auto install_runtime(Writing context, system::Device::Id device, Asset::Id asset_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.texture3arrays_id_mapping.find(asset_id); existing != runtimes.texture3arrays_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<Texture3arrayRuntime_group>::addElement(context, device, std::move(quantum));
        }

        auto layerByteSize(index3 layerSize) -> std::size_t {
            return static_cast<std::size_t>(layerSize.x) * static_cast<std::size_t>(layerSize.y) * static_cast<std::size_t>(layerSize.z) * 4u;
        }

    } // namespace

    auto Asset::Actions::install(Writing context, Id asset_id, system::Device::Id device, const CpuPresentation& cpu) -> optional<Runtime::Id> {
        const int width = static_cast<int>(cpu.layerSize.x);
        const int height = static_cast<int>(cpu.layerSize.y);
        const int depth = static_cast<int>(cpu.layerSize.z);
        if (width <= 0 or height <= 0 or depth <= 0)
            return context.refuse("resource::texture3array::Asset::install: layerSize must be positive");
        if (cpu.layers.empty())
            return context.refuse("resource::texture3array::Asset::install: no layers");

        const auto expected = layerByteSize(cpu.layerSize);
        for (const auto& layer : cpu.layers) {
            if (layer.size() != expected)
                return context.refuse("resource::texture3array::Asset::install: layer byte size mismatch");
        }

        glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);

        const int levels = 1 + static_cast<int>(std::floor(std::log2(std::max(width, std::max(height, depth)))));
        vector<renderer::Texture> handles;
        handles.reserve(cpu.layers.size());
        for (const auto& layer : cpu.layers) {
            renderer::Texture handle{};
            glCreateTextures(GL_TEXTURE_3D, 1, &handle);
            if (not handle) {
                deleteHandles(handles);
                return context.refuse("resource::texture3array::Asset::install: glCreateTextures failed");
            }
            glTextureStorage3D(handle, levels, GL_RGBA8, width, height, depth);
            glTextureSubImage3D(handle, 0, 0, 0, 0, width, height, depth, GL_RGBA, GL_UNSIGNED_BYTE, layer.data());
            glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTextureParameteri(handle, GL_TEXTURE_WRAP_R, GL_REPEAT);
            glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glGenerateTextureMipmap(handle);
            handles.push_back(handle);
        }

        const auto capacity = static_cast<integer>(cpu.layers.size());
        auto asset = with<Asset>::modify(context, asset_id);
        asset->layerSize = cpu.layerSize;
        asset->capacity = capacity;

        const auto& unit = with<Unit>::get(context, asset_id);
        base::message("rmmr: texture3array '{}' install {} layers ({}x{}x{})", unit.name.text(), capacity, width, height, depth);

        const auto runtimeId = install_runtime(context, device, asset_id, Runtime::Quantum{
            .device = device,
            .layers = std::move(handles),
            .layerSize = cpu.layerSize,
            .capacity = capacity,
        });
        with<Runtimes>::modify(context, device)->texture3arrays_id_mapping.insert_or_assign(asset_id, runtimeId);
        return runtimeId;
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
