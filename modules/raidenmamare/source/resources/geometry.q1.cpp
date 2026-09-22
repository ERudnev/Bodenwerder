#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/runtimes.q1.h>

#include "geometryAssimp.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;
    using builders::geometry::CpuPresentation;
    using builders::geometry::GeometryGenerator;

    namespace {

        struct CpuChannel {
            const std::byte* data;
            std::size_t count;
            std::size_t stride;
        };

        auto cpuChannel(const CpuPresentation& cpu, primitive::GeometrySemantics::PersistentId id) -> CpuChannel {
            const auto view = []<typename Value>(const vector<Value>& values) {
                return CpuChannel{reinterpret_cast<const std::byte*>(values.data()), values.size(), sizeof(Value)};
            };
            using Semantics = primitive::GeometrySemantics;
            if (id == Semantics::id_of("position")) return view(cpu.positions);
            if (id == Semantics::id_of("normal")) return view(cpu.normals);
            if (id == Semantics::id_of("uv0")) return view(cpu.uv0);
            if (id == Semantics::id_of("color0")) return view(cpu.color0);
            if (id == Semantics::id_of("mix0")) return view(cpu.mix0);
            if (id == Semantics::id_of("cohesion")) return view(cpu.cohesion);
            if (id == Semantics::id_of("palette")) return view(cpu.palette);
            if (id == Semantics::id_of("weights")) return view(cpu.weights);
            return CpuChannel{nullptr, 0, 0};
        }

        auto bake(Writing context, system::Device::Id device, const CpuPresentation& cpu, const vector<SurfaceId>& sourcePrimitiveSurfaces) -> Runtime::Quantum {
            using Semantics = primitive::GeometrySemantics;
            if (cpu.positions.empty()) return context.refuse("resource::geometry::bake: positions are empty");
            if (cpu.layout.empty() or cpu.layout.front() != Semantics::id_of("position")) return context.refuse("resource::geometry::bake: layout must start with position");

            const auto vertexCount = cpu.positions.size();
            struct Attrib {
                GLuint location;
                Semantics::PersistentId id;
                Semantics::Type type;
                bool live;
                const std::byte* data;
                std::size_t stride;
            };
            vector<Attrib> attribs;
            attribs.reserve(cpu.layout.size());
            for (std::size_t index = 0; index < cpu.layout.size(); ++index) {
                const auto id = cpu.layout[index];
                const auto* entry = Semantics::find(id);
                if (entry == nullptr or entry->id == Semantics::PersistentId{0}) return context.refuse("resource::geometry::bake: unknown vertex attribute");
                for (const auto& attrib : attribs) {
                    if (attrib.id == id) return context.refuse(std::format("resource::geometry::bake: duplicate vertex attribute '{}'", entry->name));
                }
                const auto channel = cpuChannel(cpu, id);
                if (channel.count != vertexCount) return context.refuse(std::format("resource::geometry::bake: {} count must match positions", entry->name));
                if (channel.stride != Semantics::byteSize(entry->type)) return context.refuse(std::format("resource::geometry::bake: {} storage does not match semantic type", entry->name));
                attribs.push_back(Attrib{.location = static_cast<GLuint>(index), .id = id, .type = entry->type, .live = entry->live, .data = channel.data, .stride = channel.stride});
            }
            for (const auto& entry : Semantics::vocabulary) {
                if (entry.id == Semantics::PersistentId{0}) continue;
                bool used = false;
                for (const auto& attrib : attribs) {
                    if (attrib.id == entry.id) {
                        used = true;
                        break;
                    }
                }
                if (used) continue;
                if (cpuChannel(cpu, entry.id).count != 0) return context.refuse(std::format("resource::geometry::bake: {} must be empty for this layout", entry.name));
            }

            vector<GLuint> indexData;
            if (not cpu.indices.empty()) {
                indexData.reserve(cpu.indices.size());
                for (const auto index : cpu.indices) {
                    if (index < 0 or static_cast<std::size_t>(index) >= vertexCount) return context.refuse("resource::geometry::bake: index out of positions range");
                    indexData.push_back(static_cast<GLuint>(index));
                }
            } else {
                indexData.reserve(vertexCount);
                for (std::size_t index = 0; index < vertexCount; ++index) indexData.push_back(static_cast<GLuint>(index));
            }

            const auto primitiveCount = indexData.size() / 3;
            vector<SurfaceId> primitiveSurfaceData = sourcePrimitiveSurfaces;
            if (primitiveSurfaceData.empty()) primitiveSurfaceData.resize(primitiveCount, SurfaceId{0});
            if (primitiveSurfaceData.size() != primitiveCount) return context.refuse("resource::geometry::bake: primitive surface count does not match triangle count");

            std::size_t packedStride = 0;
            for (const auto& attrib : attribs) {
                if (not attrib.live) packedStride += attrib.stride;
            }
            vector<std::byte> packed(vertexCount * packedStride);
            for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
                auto* dest = packed.data() + vertex * packedStride;
                std::size_t offset = 0;
                for (const auto& attrib : attribs) {
                    if (attrib.live) continue;
                    std::memcpy(dest + offset, attrib.data + vertex * attrib.stride, attrib.stride);
                    offset += attrib.stride;
                }
            }

            glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);
            renderer::VertexArray vao{};
            renderer::VertexBuffer vbo{};
            renderer::ElementBuffer ebo{};
            renderer::StorageBuffer primitiveSurfaces{};
            umap<Semantics::PersistentId, renderer::VertexBuffer> channels;
            auto release = [&] {
                if (vao) glDeleteVertexArrays(1, &vao);
                if (vbo) glDeleteBuffers(1, &vbo);
                if (ebo) glDeleteBuffers(1, &ebo);
                if (primitiveSurfaces) glDeleteBuffers(1, &primitiveSurfaces);
                for (auto& pair : channels) {
                    if (pair.second) glDeleteBuffers(1, &pair.second);
                }
            };
            glCreateVertexArrays(1, &vao);
            glCreateBuffers(1, &vbo);
            glCreateBuffers(1, &ebo);
            glCreateBuffers(1, &primitiveSurfaces);
            if (not vao or not vbo or not ebo or not primitiveSurfaces) {
                release();
                return context.refuse("resource::geometry::bake: failed to allocate VAO/VBO/EBO/primitive-surface SSBO");
            }

            auto bindAttrib = [&](const Attrib& attrib, GLuint binding, GLuint relativeOffset) {
                glEnableVertexArrayAttrib(vao, attrib.location);
                const auto components = static_cast<GLint>(Semantics::componentCount(attrib.type));
                if (Semantics::integerPacked(attrib.type)) glVertexArrayAttribIFormat(vao, attrib.location, components, GL_UNSIGNED_INT, relativeOffset);
                else glVertexArrayAttribFormat(vao, attrib.location, components, GL_FLOAT, GL_FALSE, relativeOffset);
                glVertexArrayAttribBinding(vao, attrib.location, binding);
            };

            glNamedBufferData(vbo, renderer::SizePtr(packed.size()), packed.data(), GL_STATIC_DRAW);
            glVertexArrayVertexBuffer(vao, 0, vbo, 0, renderer::Count(packedStride));
            std::size_t packedOffset = 0;
            for (const auto& attrib : attribs) {
                if (attrib.live) continue;
                bindAttrib(attrib, 0, static_cast<GLuint>(packedOffset));
                packedOffset += attrib.stride;
            }
            glNamedBufferData(ebo, renderer::SizePtr(indexData.size() * sizeof(GLuint)), indexData.data(), GL_STATIC_DRAW);
            glVertexArrayElementBuffer(vao, ebo);
            glNamedBufferData(primitiveSurfaces, renderer::SizePtr(primitiveSurfaceData.size() * sizeof(SurfaceId)), primitiveSurfaceData.data(), GL_STATIC_DRAW);

            GLuint liveBinding = 1;
            for (const auto& attrib : attribs) {
                if (not attrib.live) continue;
                renderer::VertexBuffer buffer{0};
                glCreateBuffers(1, &buffer);
                if (not buffer) {
                    release();
                    return context.refuse("resource::geometry::bake: failed to allocate attrib channel");
                }
                glNamedBufferData(buffer, renderer::SizePtr(vertexCount * attrib.stride), attrib.data, GL_STATIC_DRAW);
                bindAttrib(attrib, liveBinding, 0);
                glVertexArrayVertexBuffer(vao, liveBinding, buffer, 0, renderer::Count(attrib.stride));
                channels.insert_or_assign(attrib.id, buffer);
                liveBinding += 1;
            }

            vec3 boundMin = cpu.positions.front();
            vec3 boundMax = boundMin;
            for (const auto& position : cpu.positions) {
                boundMin = glm::min(boundMin, vec3{position});
                boundMax = glm::max(boundMax, vec3{position});
            }
            return Runtime::Quantum{
                .device = device,
                .vao = vao,
                .vbo = vbo,
                .channels = std::move(channels),
                .ebo = ebo,
                .primitiveSurfaces = primitiveSurfaces,
                .vertex_count = renderer::Count(vertexCount),
                .index_count = renderer::Count(indexData.size()),
                .boundMin = boundMin,
                .boundMax = boundMax,
            };
        }

        auto cpu_for(const Generator::Quantum& generator) -> CpuPresentation {
            switch (generator.type) {
                case Generator::Type::triangle: return GeometryGenerator::triangle();
                case Generator::Type::kube: return GeometryGenerator::kube();
                case Generator::Type::bagel: return GeometryGenerator::bagel();
                case Generator::Type::gridPlane: return GeometryGenerator::gridPlane();
                case Generator::Type::unitQuad: return GeometryGenerator::unitQuad();
                case Generator::Type::sphere: return GeometryGenerator::sphere(generator.subdivisions);
                case Generator::Type::diamond: return GeometryGenerator::diamond();
                case Generator::Type::patchGrid: return GeometryGenerator::patchGrid(generator.subdivisions);
            }
        }

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (not last.vao && not last.vbo && last.channels.empty() && not last.ebo && not last.primitiveSurfaces) {
                return;
            }
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            if (last.vao) {
                auto vao = last.vao;
                glDeleteVertexArrays(1, &vao);
            }
            if (last.vbo) {
                auto vbo = last.vbo;
                glDeleteBuffers(1, &vbo);
            }
            for (const auto& channel : last.channels) {
                auto buffer = channel.second;
                if (buffer)
                    glDeleteBuffers(1, &buffer);
            }
            if (last.ebo) {
                auto ebo = last.ebo;
                glDeleteBuffers(1, &ebo);
            }
            if (last.primitiveSurfaces) {
                auto primitiveSurfaces = last.primitiveSurfaces;
                glDeleteBuffers(1, &primitiveSurfaces);
            }
        }

        auto install_runtime(Writing context, system::Device::Id device, Asset::Id asset_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.geometries_id_mapping.find(asset_id); existing != runtimes.geometries_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<GeometryRuntime_group>::addElement(context, device, std::move(quantum));
        }

        void setSingleEntry(Writing context, Asset::Id assetId, const CpuPresentation& cpu) {
            const auto indexCount = static_cast<renderer::Count>(cpu.indices.empty() ? cpu.positions.size() : cpu.indices.size());
            auto asset = with<Asset>::modify(context, assetId);
            asset->entries = {Asset::Entry{
                .vertices = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(cpu.positions.size())},
                .indices = Asset::Range{.first = renderer::Count{0}, .count = indexCount},
                .surfaces = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{1}},
                .mounts = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}},
                .origin = vec3{0.0f, 0.0f, 0.0f},
            }};
            asset->surfaces = {Asset::Surface{.indices = Asset::Range{.first = renderer::Count{0}, .count = indexCount}}};
            asset->mounts = {};
            asset->entryCatalog = {{"mesh", EntryId{0}}};
            asset->surfaceCatalogs = {{{"surface", SurfaceId{0}}}};
        }

        void setCataloguedEntry(Writing context, Asset::Id assetId, const CpuPresentation& cpu, const vector<SurfaceId>& primitiveSurfaces, const umap<string, SurfaceId>& catalog) {
            const auto indexCount = static_cast<renderer::Count>(cpu.indices.empty() ? cpu.positions.size() : cpu.indices.size());
            SurfaceId surfaceCount{0};
            for (const auto surface : primitiveSurfaces)
                if (surface >= surfaceCount) surfaceCount = surface + 1;
            vector<Asset::Surface> surfaces(static_cast<std::size_t>(surfaceCount), Asset::Surface{.indices = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}}});
            for (std::size_t triangle = 0; triangle < primitiveSurfaces.size(); ++triangle) {
                auto& range = surfaces[static_cast<std::size_t>(primitiveSurfaces[triangle])].indices;
                const auto first = static_cast<renderer::Count>(triangle * 3);
                if (range.count == renderer::Count{0})
                    range.first = first;
                range.count += renderer::Count{3};
            }
            auto asset = with<Asset>::modify(context, assetId);
            asset->entries = {Asset::Entry{
                .vertices = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(cpu.positions.size())},
                .indices = Asset::Range{.first = renderer::Count{0}, .count = indexCount},
                .surfaces = Asset::Range{.first = renderer::Count{0}, .count = static_cast<renderer::Count>(surfaceCount)},
                .mounts = Asset::Range{.first = renderer::Count{0}, .count = renderer::Count{0}},
                .origin = vec3{0.0f, 0.0f, 0.0f},
            }};
            asset->surfaces = std::move(surfaces);
            asset->mounts = {};
            asset->entryCatalog = {{"mesh", EntryId{0}}};
            asset->surfaceCatalogs = {catalog};
        }

        template<typename Context>
        void writeChannelOn(Context context, Runtime::Id id, primitive::GeometrySemantics::PersistentId semantic, const void* data, renderer::SizePtr bytes) {
            if (semantic == primitive::GeometrySemantics::PersistentId{0} or data == nullptr or bytes == 0 or not with<Runtime>::exists(context, id))
                return;
            const auto& runtime = with<Runtime>::get(context, id);
            const auto found = runtime.channels.find(semantic);
            if (found == runtime.channels.end() or not found->second)
                return;
            if (bytes != static_cast<renderer::SizePtr>(static_cast<std::size_t>(runtime.vertex_count) * sizeof(float)))
                return;
            glfwMakeContextCurrent(with<system::Device>::get(context, runtime.device).handle);
            glNamedBufferSubData(found->second, 0, bytes, data);
        }

    } // namespace

    void Asset::Actions::writeChannel(Writing context, Runtime::Id id, primitive::GeometrySemantics::PersistentId semantic, const void* data, renderer::SizePtr bytes) {
        writeChannelOn(context, id, semantic, data, bytes);
    }

    void Asset::Actions::writeChannel(Stewarding context, Runtime::Id id, primitive::GeometrySemantics::PersistentId semantic, const void* data, renderer::SizePtr bytes) {
        writeChannelOn(context, id, semantic, data, bytes);
    }

    auto Asset::Actions::install(Writing context, Id asset_id, system::Device::Id device, const CpuPresentation& cpu) -> optional<Runtime::Id> {
        setSingleEntry(context, asset_id, cpu);
        auto quantum = bake(context, device, cpu, {});
        if (not quantum.vao) {
            return {};
        }
        const auto runtime_id = install_runtime(context, device, asset_id, std::move(quantum));
        with<Runtimes>::modify(context, device)->geometries_id_mapping.insert_or_assign(asset_id, runtime_id);
        return runtime_id;
    }

    auto Asset::Actions::install(Writing context, Id asset_id, system::Device::Id device, const CpuPresentation& cpu, const vector<SurfaceId>& primitiveSurfaces, const umap<string, SurfaceId>& surfaceCatalog) -> optional<Runtime::Id> {
        const auto indexCount = cpu.indices.empty() ? cpu.positions.size() : cpu.indices.size();
        if (cpu.positions.empty() or indexCount % 3 != 0)
            return context.refuse("resource::geometry::Asset::install: mesh is empty or not triangulated");
        if (primitiveSurfaces.size() != indexCount / 3)
            return context.refuse("resource::geometry::Asset::install: primitive surface count does not match triangle count");
        if (surfaceCatalog.empty())
            return context.refuse("resource::geometry::Asset::install: surface catalog is empty");
        setCataloguedEntry(context, asset_id, cpu, primitiveSurfaces, surfaceCatalog);
        auto quantum = bake(context, device, cpu, primitiveSurfaces);
        if (not quantum.vao)
            return {};
        const auto runtime_id = install_runtime(context, device, asset_id, std::move(quantum));
        with<Runtimes>::modify(context, device)->geometries_id_mapping.insert_or_assign(asset_id, runtime_id);
        return runtime_id;
    }

    auto Loader::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& loader = with<Loader>::get(context, asset_id);
        const auto& unit = with<Unit>::get(context, asset_id);
        const auto path = with<Manager>::resolve(context, unit, loader.file);
        base::whisper("rmmr: geometry::Loader '{}' ← {}", unit.name.text(), path.string());

        const auto loaded = assimp::load(path);
        if (not loaded) {
            return context.refuse(std::format("resource::geometry::Loader::materialize: Assimp failed '{}'", path.string()));
        }

        {
            auto asset = with<Asset>::modify(context, asset_id);
            asset->entries = loaded->entries;
            asset->surfaces = loaded->surfaces;
            asset->mounts = loaded->mounts;
            asset->entryCatalog = loaded->entryCatalog;
            asset->surfaceCatalogs = loaded->surfaceCatalogs;
        }
        auto quantum = bake(context, device, loaded->cpu, loaded->primitiveSurfaces);
        if (not quantum.vao) return {};
        const auto runtimeId = install_runtime(context, device, asset_id, std::move(quantum));
        with<Runtimes>::modify(context, device)->geometries_id_mapping.insert_or_assign(asset_id, runtimeId);
        return runtimeId;
    }

    auto Generator::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& generator = with<Generator>::get(context, asset_id);
        const auto cpu = cpu_for(generator);
        setSingleEntry(context, asset_id, cpu);
        auto quantum = bake(context, device, cpu, {});
        if (not quantum.vao) {
            return {};
        }
        return install_runtime(context, device, asset_id, std::move(quantum));
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
