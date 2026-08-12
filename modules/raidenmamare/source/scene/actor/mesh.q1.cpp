#include <rmmr/scene/actors/mesh.q1.h>

#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/sprite.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    namespace {

        struct CpuBucket {
            resource::geometry::Runtime::Id geometry;
            resource::material::Runtime::Id material;
            base::maybe<resource::texpack::Runtime::Id> texpack;
            vector<renderer::DrawElementsIndirect> commands;
            vector<renderer::DrawMetadata> metadata;
        };

        auto findBucket(vector<CpuBucket>& buckets, resource::geometry::Runtime::Id geometry, resource::material::Runtime::Id material, base::maybe<resource::texpack::Runtime::Id> texpack) -> CpuBucket& {
            const auto found = std::find_if(buckets.begin(), buckets.end(), [&](const CpuBucket& bucket) {
                const bool sameTexpack = (not bucket.texpack and not texpack) or (bucket.texpack and texpack and *bucket.texpack == *texpack);
                return bucket.geometry == geometry and bucket.material == material and sameTexpack;
            });
            if (found != buckets.end()) return *found;
            buckets.push_back(CpuBucket{.geometry = geometry, .material = material, .texpack = texpack, .commands = {}, .metadata = {}});
            return buckets.back();
        }

        auto texpackRuntime(Reading context, const resource::Runtimes::Quantum& runtimes, base::maybe<resource::texpack::Pack::Id> texpack) -> base::maybe<resource::texpack::Runtime::Id> {
            if (not texpack) return {};
            const auto found = runtimes.texpacks_id_mapping.find(*texpack);
            if (found == runtimes.texpacks_id_mapping.end() or not with<resource::texpack::Runtime>::exists(context, found->second)) return {};
            return found->second;
        }

        auto layerIndex(Reading context, const resource::material::Instance& instance, base::maybe<resource::texpack::Runtime::Id> texpack) -> optional<renderer::Integer32> {
            const auto texture = instance.textures.find("albedoMap");
            if (texture == instance.textures.end()) return renderer::Integer32{0};
            if (not texpack) return {};
            const auto& runtime = with<resource::texpack::Runtime>::get(context, *texpack);
            const auto layer = runtime.layers.find(texture->second);
            if (layer == runtime.layers.end() or layer->second < 0) return {};
            return static_cast<renderer::Integer32>(layer->second);
        }

        void deleteBuffers(renderer::StorageBuffer actorState, renderer::StorageBuffer poses, renderer::StorageBuffer metadata, renderer::StorageBuffer palette, const vector<Mesh::Bucket>& buckets) {
            if (actorState) glDeleteBuffers(1, &actorState);
            if (poses) glDeleteBuffers(1, &poses);
            if (metadata) glDeleteBuffers(1, &metadata);
            if (palette) glDeleteBuffers(1, &palette);
            for (const auto& bucket : buckets) {
                if (bucket.indirect) {
                    auto indirect = bucket.indirect;
                    glDeleteBuffers(1, &indirect);
                }
            }
        }

        auto gpuBatch(const Mesh::Quantum& mesh, const Mesh::Bucket& bucket, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, base::maybe<resource::texpack::Runtime::Id> texpack, renderer::BlendMode blend) -> renderer::GpuBatch {
            return renderer::GpuBatch{
                .geometry = bucket.geometry,
                .material = material,
                .shader = shader,
                .texpack = texpack,
                .sprite = mesh.sprite,
                .actorState = mesh.actorState,
                .poses = mesh.poses,
                .drawMetadata = mesh.drawMetadata,
                .surfacePalette = mesh.surfacePalette,
                .metadataByteOffset = bucket.metadataByteOffset,
                .metadataByteSize = bucket.metadataByteSize,
                .indirect = bucket.indirect,
                .drawCount = bucket.drawCount,
                .renderState = renderer::RenderState{.blend = blend},
            };
        }

        auto actorState(Reading context, Mesh::Id node, const Mesh::Quantum& mesh, const MeshState::Quantum& meshState) -> renderer::ActorState {
            auto modelScale = meshState.scale;
            auto albedo = meshState.albedo;
            auto opacity = meshState.opacity;
            auto spriteIndex = static_cast<renderer::Integer32>(mesh.spriteIndex);
            if (with<Sprite>::exists(context, node)) {
                const auto& sprite = with<Sprite>::get(context, node);
                modelScale = sprite.scale;
                albedo = RGB{1.0f, 1.0f, 1.0f} + sprite.tint;
                opacity = sprite.opacity;
                spriteIndex = static_cast<renderer::Integer32>(sprite.index);
            }
            auto scenicAlias = renderer::Integer32{0};
            if (with<Identified>::exists(context, node)) scenicAlias = with<Identified>::get(context, node).scenicAlias;
            return renderer::ActorState{
                .model = glm::scale(Node::Actions::transform(context, node), modelScale),
                .albedoOpacity = vec4{albedo, opacity},
                .latticePattern = vec2{meshState.latticeStep, meshState.patternScale},
                .scenicAlias = scenicAlias,
                .spriteIndex = spriteIndex,
            };
        }

        auto primaryDevice(Reading context) -> optional<system::Device::Id> {
            for (const auto entry : context->aspect<system::Device>().items()) return entry.id;
            return {};
        }

    } // namespace

    auto Mesh::Actions::compose(Reading context, const vector<Occurrence>& occurrences) -> optional<Quantum> {
        const auto device = primaryDevice(context);
        if (occurrences.empty() or not device or not with<resource::Runtimes>::exists(context, *device)) return {};
        const auto& runtimes = with<resource::Runtimes>::get(context, *device);
        vector<renderer::DiscretePose> poses;
        vector<renderer::Integer32> palette;
        vector<CpuBucket> cpuBuckets;
        poses.reserve(occurrences.size());

        for (const auto& occurrence : occurrences) {
            const auto& resolved = occurrence.entry;
            if (not with<resource::geometry::Asset>::exists(context, resolved.geometry)) return {};
            const auto geometryRuntimeFound = runtimes.geometries_id_mapping.find(resolved.geometry);
            if (geometryRuntimeFound == runtimes.geometries_id_mapping.end() or not with<resource::geometry::Runtime>::exists(context, geometryRuntimeFound->second)) return {};
            const auto& geometryRuntime = with<resource::geometry::Runtime>::get(context, geometryRuntimeFound->second);
            if (not geometryRuntime.ebo) return {};
            const auto& geometry = with<resource::geometry::Asset>::get(context, resolved.geometry);
            if (resolved.entry >= geometry.entries.size()) return {};
            const auto& entry = geometry.entries[resolved.entry];
            const auto poseIndex = static_cast<renderer::Integer32>(poses.size());
            const auto surfaceBase = static_cast<renderer::Integer32>(palette.size());
            poses.push_back(occurrence.pose);
            palette.resize(palette.size() + geometry.surfaces.size(), std::numeric_limits<renderer::Integer32>::max());
            const auto packRuntime = texpackRuntime(context, runtimes, resolved.texpack);
            if (resolved.texpack and not packRuntime) return {};

            for (const auto& [surfaceId, instance] : resolved.surfaces) {
                if (surfaceId < static_cast<resource::geometry::SurfaceId>(entry.surfaces.first) or surfaceId >= static_cast<resource::geometry::SurfaceId>(entry.surfaces.first + entry.surfaces.count)) return {};
                if (surfaceId >= geometry.surfaces.size()) return {};
                const auto materialFound = runtimes.materials_id_mapping.find(instance.material);
                if (materialFound == runtimes.materials_id_mapping.end() or not with<resource::material::Runtime>::exists(context, materialFound->second)) return {};
                const auto layer = layerIndex(context, instance, packRuntime);
                if (not layer) return {};
                palette[static_cast<std::size_t>(surfaceBase) + surfaceId] = *layer;
                const auto& surface = geometry.surfaces[surfaceId];
                if (surface.indices.count <= renderer::Count{0}) continue;
                auto& bucket = findBucket(cpuBuckets, geometryRuntimeFound->second, materialFound->second, packRuntime);
                bucket.commands.push_back(renderer::DrawElementsIndirect{
                    .count = static_cast<renderer::Integer32>(surface.indices.count),
                    .instanceCount = renderer::Integer32{1},
                    .firstIndex = static_cast<renderer::Integer32>(surface.indices.first),
                    .baseVertex = renderer::Signed32{0},
                    .baseInstance = poseIndex,
                });
                bucket.metadata.push_back(renderer::DrawMetadata{
                    .primitiveBase = static_cast<renderer::Integer32>(surface.indices.first / 3),
                    .surfaceBase = surfaceBase,
                });
            }
        }

        if (cpuBuckets.empty()) return {};
        glfwMakeContextCurrent(with<system::Device>::get(context, *device).handle);
        GLint metadataAlignment = 1;
        glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &metadataAlignment);
        const auto metadataStride = sizeof(renderer::DrawMetadata);
        const auto alignment = static_cast<std::size_t>(std::max(metadataAlignment, 1));
        vector<renderer::DrawMetadata> metadata;
        vector<std::size_t> metadataOffsets;
        metadataOffsets.reserve(cpuBuckets.size());
        for (const auto& bucket : cpuBuckets) {
            const auto byteSize = metadata.size() * metadataStride;
            const auto alignedByteSize = (byteSize + alignment - 1) / alignment * alignment;
            metadata.resize((alignedByteSize + metadataStride - 1) / metadataStride, renderer::DrawMetadata{.primitiveBase = renderer::Integer32{0}, .surfaceBase = renderer::Integer32{0}});
            metadataOffsets.push_back(metadata.size() * metadataStride);
            metadata.insert(metadata.end(), bucket.metadata.begin(), bucket.metadata.end());
        }

        renderer::StorageBuffer actorStateBuffer{0};
        renderer::StorageBuffer poseBuffer{0};
        renderer::StorageBuffer metadataBuffer{0};
        renderer::StorageBuffer paletteBuffer{0};
        glCreateBuffers(1, &actorStateBuffer);
        glCreateBuffers(1, &poseBuffer);
        glCreateBuffers(1, &metadataBuffer);
        glCreateBuffers(1, &paletteBuffer);
        vector<Bucket> buckets;
        if (not actorStateBuffer or not poseBuffer or not metadataBuffer or not paletteBuffer) {
            deleteBuffers(actorStateBuffer, poseBuffer, metadataBuffer, paletteBuffer, buckets);
            return {};
        }
        const auto initialActorState = renderer::ActorState{
            .model = mat4{1.0f},
            .albedoOpacity = vec4{1.0f, 1.0f, 1.0f, 1.0f},
            .latticePattern = vec2{1.0f, 1.0f},
            .scenicAlias = renderer::Integer32{0},
            .spriteIndex = renderer::Integer32{0},
        };
        glNamedBufferStorage(actorStateBuffer, sizeof(renderer::ActorState), &initialActorState, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferStorage(poseBuffer, static_cast<renderer::SizePtr>(poses.size() * sizeof(renderer::DiscretePose)), poses.data(), 0);
        glNamedBufferStorage(metadataBuffer, static_cast<renderer::SizePtr>(metadata.size() * sizeof(renderer::DrawMetadata)), metadata.data(), 0);
        glNamedBufferStorage(paletteBuffer, static_cast<renderer::SizePtr>(palette.size() * sizeof(renderer::Integer32)), palette.data(), 0);

        buckets.reserve(cpuBuckets.size());
        for (std::size_t index = 0; index < cpuBuckets.size(); ++index) {
            renderer::IndirectBuffer indirect{0};
            glCreateBuffers(1, &indirect);
            if (not indirect) {
                deleteBuffers(actorStateBuffer, poseBuffer, metadataBuffer, paletteBuffer, buckets);
                return {};
            }
            const auto& source = cpuBuckets[index];
            glNamedBufferStorage(indirect, static_cast<renderer::SizePtr>(source.commands.size() * sizeof(renderer::DrawElementsIndirect)), source.commands.data(), 0);
            buckets.push_back(Bucket{
                .geometry = source.geometry,
                .material = source.material,
                .texpack = source.texpack,
                .indirect = indirect,
                .drawCount = static_cast<renderer::Count>(source.commands.size()),
                .metadataByteOffset = static_cast<renderer::IntPtr>(metadataOffsets[index]),
                .metadataByteSize = static_cast<renderer::SizePtr>(source.metadata.size() * sizeof(renderer::DrawMetadata)),
            });
        }
        return Quantum{.device = *device, .actorState = actorStateBuffer, .poses = poseBuffer, .drawMetadata = metadataBuffer, .surfacePalette = paletteBuffer, .sprite = {}, .spriteIndex = 0, .buckets = std::move(buckets)};
    }

    auto Mesh::Actions::compose(Reading context, resource::meshpack::Asset::Resolved resolved) -> optional<Quantum> {
        return compose(context, {Occurrence{.entry = std::move(resolved), .pose = renderer::DiscretePose::identity()}});
    }

    auto Mesh::Actions::composeOne(Reading context, resource::geometry::Asset::Id geometryId, resource::material::Asset::Id material) -> optional<Quantum> {
        if (not with<resource::geometry::Asset>::exists(context, geometryId)) return {};
        const auto& geometry = with<resource::geometry::Asset>::get(context, geometryId);
        if (geometry.entries.empty()) return {};
        const auto& entry = geometry.entries.front();
        umap<resource::geometry::SurfaceId, resource::material::Instance> surfaces;
        for (renderer::Count offset = 0; offset < entry.surfaces.count; ++offset) {
            const auto surface = static_cast<resource::geometry::SurfaceId>(entry.surfaces.first + offset);
            surfaces.emplace(surface, resource::material::Instance{.material = material, .textures = {}});
        }
        return compose(context, resource::meshpack::Asset::Resolved{.geometry = geometryId, .entry = resource::geometry::EntryId{0}, .surfaces = std::move(surfaces), .texpack = {}});
    }

    auto MeshState::Actions::defaults() -> Quantum {
        return Quantum{.albedo = RGB{1.0f, 1.0f, 1.0f}, .scale = vec3{1.0f, 1.0f, 1.0f}, .latticeStep = 1.0f, .patternScale = 1.0f, .opacity = 1.0f, .visible = true};
    }

    auto MeshState::Actions::defaults(RGB albedo, float opacity) -> Quantum {
        return Quantum{.albedo = albedo, .scale = vec3{1.0f, 1.0f, 1.0f}, .latticeStep = 1.0f, .patternScale = 1.0f, .opacity = opacity, .visible = true};
    }

    auto MeshState::Actions::defaults(RGB albedo, float opacity, vec3 scale) -> Quantum {
        return Quantum{.albedo = albedo, .scale = scale, .latticeStep = 1.0f, .patternScale = 1.0f, .opacity = opacity, .visible = true};
    }

    void Mesh::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& mesh = with<Mesh>::get(context, node);
        const auto& state = with<MeshState>::get(context, node);
        if (not state.visible or mesh.device != device) return;
        const auto gpuState = actorState(context, node, mesh, state);
        glNamedBufferSubData(mesh.actorState, 0, sizeof(renderer::ActorState), &gpuState);
        for (const auto& bucket : mesh.buckets) {
            const auto& material = with<resource::material::Runtime>::get(context, bucket.material);
            for (const auto& [pass, technique] : material.techniques) {
                where.gpu[pass].push_back(gpuBatch(mesh, bucket, bucket.material, technique.shader, bucket.texpack, material.blend));
            }
        }
    }

    void MeshState::Actions::setVisible(Writing context, Id node, bool visible) {
        with<MeshState>::modify(context, node)->visible = visible;
    }

    void Identified::Actions::extend(Writing context, Mesh::Id mesh) {
        auto global = with<Identified>::modify_global(context);
        ++global->lastGeneratedId;
        const auto alias = static_cast<renderer::Integer32>(global->lastGeneratedId);
        Identified::BaseActions::extend(context, mesh, Identified::Quantum{.scenicAlias = alias, .selected = false});
    }

    auto Identified::Actions::lookup(Reading context, renderer::Integer32 alias) -> optional<Id> {
        if (alias == renderer::Integer32{0}) return {};
        for (const auto [id, quantum] : context->aspect<Identified>().items()) {
            if (quantum.scenicAlias == alias) return id;
        }
        return {};
    }

    void Identified::Actions::applySelection(Writing context, const vector<renderer::Integer32>& aliases) {
        // Diff only: stable selection → zero Writing. Old path cleared every Identified every frame.
        std::unordered_set<renderer::Integer32> wanted;
        wanted.reserve(aliases.size());
        for (const auto alias : aliases) {
            if (alias != renderer::Integer32{0})
                wanted.insert(alias);
        }
        vector<std::pair<Id, bool>> flips;
        for (const auto [id, quantum] : context->aspect<Identified>().items()) {
            const bool on = wanted.contains(quantum.scenicAlias);
            if (quantum.selected != on)
                flips.emplace_back(id, on);
        }
        for (const auto& [id, on] : flips)
            with<Identified>::modify(context, id)->selected = on;
    }

    void Identified::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& mesh = with<Mesh>::get(context, node);
        const auto& state = with<MeshState>::get(context, node);
        const auto& global = with<Identified>::get_global(context);
        if (not state.visible or mesh.device != device or not global.material) return;
        const auto& runtimes = with<resource::Runtimes>::get(context, device);
        const auto materialFound = runtimes.materials_id_mapping.find(*global.material);
        if (materialFound == runtimes.materials_id_mapping.end()) throw std::runtime_error("scene::actor::Identified: identity material runtime missing");
        const auto& material = with<resource::material::Runtime>::get(context, materialFound->second);
        const auto technique = material.techniques.find(renderer::Pass::identity);
        if (technique == material.techniques.end()) throw std::runtime_error("scene::actor::Identified: identity technique missing");
        const auto& identified = with<Identified>::get(context, node);
        for (const auto& bucket : mesh.buckets) {
            const auto batch = gpuBatch(mesh, bucket, materialFound->second, technique->second.shader, {}, material.blend);
            if (identified.selected) where.gpu[renderer::Pass::identitySelected].push_back(batch);
            where.gpu[renderer::Pass::identity].push_back(batch);
        }
    }

    struct Mesh::Internals : Mesh::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            if (not with<system::Device>::exists(context, last.device)) return;
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            deleteBuffers(last.actorState, last.poses, last.drawMetadata, last.surfacePalette, last.buckets);
        }
    };

    auto Mesh::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Mesh>(&Mesh::Internals::release),
        };
    }

}
