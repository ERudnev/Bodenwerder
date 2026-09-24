#include <rmmr/scene/actors/patchGrid.q1.h>

#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/ext/vector_int4.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    namespace {

        static_assert(sizeof(PatchGrid::FieldState) == 464);
        static_assert(sizeof(PatchGrid::GpuTile) == 32);

        auto packTiles(std::span<const PatchGrid::Patch> patches) -> vector<PatchGrid::GpuTile> {
            vector<PatchGrid::GpuTile> packed;
            packed.reserve(patches.size());
            for (const auto& patch : patches)
                packed.push_back(PatchGrid::GpuTile{
                    .loc = glm::ivec4{static_cast<int>(patch.diamond), static_cast<int>(patch.originU), static_cast<int>(patch.originV), static_cast<int>(patch.step)},
                    .neighbors = glm::ivec4{static_cast<int>(patch.stepNegU), static_cast<int>(patch.stepPosU), static_cast<int>(patch.stepNegV), static_cast<int>(patch.stepPosV)},
                });
            return packed;
        }

        auto maxPatchCapacity(integer span, integer cells) -> integer {
            const integer segments = std::max(span - 1, integer{1});
            const integer buckets = std::max((segments + cells - 1) / std::max(cells, integer{1}), integer{1});
            return 10 * buckets * buckets;
        }

        auto gpuBatch(const PatchGrid::Quantum& grid, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, renderer::RenderState renderState) -> renderer::GpuBatch {
            return renderer::GpuBatch{
                .geometry = grid.geometry,
                .material = material,
                .shader = shader,
                .texpack = grid.texpack,
                .texture3array = {},
                .heightField = grid.heightField,
                .coverField = grid.coverField,
                .farAlbedoField = grid.farAlbedoField,
                .farNormalField = grid.farNormalField,
                .sprite = {},
                .actorState = grid.actorState,
                .poses = grid.patches,
                .cohesions = grid.dummy,
                .heats = grid.dummy,
                .drawMetadata = grid.dummy,
                .surfacePalette = grid.dummy,
                .metadataByteOffset = renderer::IntPtr{0},
                .metadataByteSize = renderer::SizePtr{16},
                .indirect = grid.indirect,
                .drawCount = grid.drawCount,
                .renderState = renderState,
            };
        }

        auto fieldState(Reading context, PatchGrid::Id node, const PatchGrid::Quantum& grid) -> PatchGrid::FieldState {
            PatchGrid::FieldState state{};
            state.model = Node::Actions::transform(context, node);
            state.albedoOpacity = vec4{1.0f, 1.0f, 1.0f, 1.0f};
            state.radius = grid.radius;
            state.amplitude = grid.amplitude;
            state.span = static_cast<std::int32_t>(grid.span);
            state.cells = static_cast<std::int32_t>(grid.cells);
            state.lod = vec4{grid.firstLodDistance, 0.0f, 0.0f, 0.0f};
            for (integer index = 0; index < 12; ++index)
                state.shell[index] = grid.shell.vertices[static_cast<std::size_t>(index)];
            for (integer index = 0; index < 10; ++index)
                state.diamonds[index] = grid.shell.diamonds[static_cast<std::size_t>(index)];
            return state;
        }

        void deleteBuffers(const PatchGrid::Quantum& last) {
            if (last.actorState) {
                auto buffer = last.actorState;
                glDeleteBuffers(1, &buffer);
            }
            if (last.patches) {
                auto buffer = last.patches;
                glDeleteBuffers(1, &buffer);
            }
            if (last.dummy) {
                auto buffer = last.dummy;
                glDeleteBuffers(1, &buffer);
            }
            if (last.indirect) {
                auto buffer = last.indirect;
                glDeleteBuffers(1, &buffer);
            }
        }

        auto primaryDevice(Reading context) -> optional<system::Device::Id> {
            for (const auto entry : context->aspect<system::Device>().items()) return entry.id;
            return {};
        }

    }

    auto PatchGrid::Actions::compose(Reading context, resource::geometry::Asset::Id geometryId, resource::material::Asset::Id materialId, resource::texpack::Pack::Id packId, resource::texture::Asset::Id heightId, resource::texture::Asset::Id coverId, resource::texture::Asset::Id farAlbedoId, resource::texture::Asset::Id farNormalId, const Shell& shell, std::span<const Patch> patches, float radius, float amplitude, float firstLodDistance, integer span, integer cells) -> optional<Quantum> {
        const auto device = primaryDevice(context);
        if (not device or span < 2 or cells < 1 or not with<resource::Runtimes>::exists(context, *device))
            return {};
        const auto& runtimes = with<resource::Runtimes>::get(context, *device);
        const auto geometryFound = runtimes.geometries_id_mapping.find(geometryId);
        const auto materialFound = runtimes.materials_id_mapping.find(materialId);
        const auto packFound = runtimes.texpacks_id_mapping.find(packId);
        const auto heightFound = runtimes.textures_id_mapping.find(heightId);
        const auto coverFound = runtimes.textures_id_mapping.find(coverId);
        const auto farAlbedoFound = runtimes.textures_id_mapping.find(farAlbedoId);
        const auto farNormalFound = runtimes.textures_id_mapping.find(farNormalId);
        if (geometryFound == runtimes.geometries_id_mapping.end() or materialFound == runtimes.materials_id_mapping.end() or packFound == runtimes.texpacks_id_mapping.end() or heightFound == runtimes.textures_id_mapping.end() or coverFound == runtimes.textures_id_mapping.end() or farAlbedoFound == runtimes.textures_id_mapping.end() or farNormalFound == runtimes.textures_id_mapping.end())
            return {};
        if (not with<resource::geometry::Runtime>::exists(context, geometryFound->second) or not with<resource::material::Runtime>::exists(context, materialFound->second) or not with<resource::texpack::Runtime>::exists(context, packFound->second) or not with<resource::texture::Runtime>::exists(context, heightFound->second) or not with<resource::texture::Runtime>::exists(context, coverFound->second) or not with<resource::texture::Runtime>::exists(context, farAlbedoFound->second) or not with<resource::texture::Runtime>::exists(context, farNormalFound->second))
            return {};
        const auto& geometry = with<resource::geometry::Runtime>::get(context, geometryFound->second);
        if (not geometry.ebo or geometry.index_count <= renderer::Count{0})
            return {};
        const integer capacity = std::max(maxPatchCapacity(span, cells), integer{1});
        auto packed = packTiles(patches.size() > static_cast<std::size_t>(capacity) ? patches.first(static_cast<std::size_t>(capacity)) : patches);
        glfwMakeContextCurrent(with<system::Device>::get(context, *device).handle);
        renderer::StorageBuffer actorState{0};
        renderer::StorageBuffer patchBuffer{0};
        renderer::StorageBuffer dummy{0};
        renderer::IndirectBuffer indirect{0};
        glCreateBuffers(1, &actorState);
        glCreateBuffers(1, &patchBuffer);
        glCreateBuffers(1, &dummy);
        glCreateBuffers(1, &indirect);
        if (not actorState or not patchBuffer or not dummy or not indirect) {
            if (actorState) glDeleteBuffers(1, &actorState);
            if (patchBuffer) glDeleteBuffers(1, &patchBuffer);
            if (dummy) glDeleteBuffers(1, &dummy);
            if (indirect) glDeleteBuffers(1, &indirect);
            return {};
        }
        FieldState initial{};
        initial.model = mat4{1.0f};
        initial.albedoOpacity = vec4{1.0f, 1.0f, 1.0f, 1.0f};
        initial.radius = radius;
        initial.amplitude = amplitude;
        initial.span = static_cast<std::int32_t>(span);
        initial.cells = static_cast<std::int32_t>(cells);
        initial.lod = vec4{firstLodDistance, 0.0f, 0.0f, 0.0f};
        for (integer index = 0; index < 12; ++index)
            initial.shell[index] = shell.vertices[static_cast<std::size_t>(index)];
        for (integer index = 0; index < 10; ++index)
            initial.diamonds[index] = shell.diamonds[static_cast<std::size_t>(index)];
        const renderer::DrawElementsIndirect command{
            .count = static_cast<renderer::Integer32>(geometry.index_count),
            .instanceCount = static_cast<renderer::Integer32>(packed.size()),
            .firstIndex = renderer::Integer32{0},
            .baseVertex = renderer::Signed32{0},
            .baseInstance = renderer::Integer32{0},
        };
        std::uint32_t dummyBytes[4]{0, 0, 0, 0};
        glNamedBufferStorage(actorState, sizeof(FieldState), &initial, GL_DYNAMIC_STORAGE_BIT);
        glNamedBufferStorage(patchBuffer, static_cast<renderer::SizePtr>(capacity) * static_cast<renderer::SizePtr>(sizeof(GpuTile)), nullptr, GL_DYNAMIC_STORAGE_BIT);
        if (not packed.empty())
            glNamedBufferSubData(patchBuffer, 0, static_cast<renderer::SizePtr>(packed.size() * sizeof(GpuTile)), packed.data());
        glNamedBufferStorage(dummy, sizeof(dummyBytes), dummyBytes, 0);
        glNamedBufferStorage(indirect, sizeof(command), &command, GL_DYNAMIC_STORAGE_BIT);
        return Quantum{
            .device = *device,
            .actorState = actorState,
            .patches = patchBuffer,
            .dummy = dummy,
            .patchCount = static_cast<integer>(packed.size()),
            .patchCapacity = capacity,
            .radius = radius,
            .amplitude = amplitude,
            .firstLodDistance = firstLodDistance,
            .span = span,
            .cells = cells,
            .shell = shell,
            .geometry = geometryFound->second,
            .material = materialFound->second,
            .texpack = packFound->second,
            .heightField = heightFound->second,
            .coverField = coverFound->second,
            .farAlbedoField = farAlbedoFound->second,
            .farNormalField = farNormalFound->second,
            .indirect = indirect,
            .drawCount = renderer::Count{1},
        };
    }

    void PatchGrid::Actions::setPatches(Writing context, Id node, std::span<const Patch> patches) {
        if (not with<PatchGrid>::exists(context, node))
            return;
        auto grid = with<PatchGrid>::modify(context, node);
        if (grid->patchCapacity < 1 or not with<system::Device>::exists(context, grid->device) or not with<resource::geometry::Runtime>::exists(context, grid->geometry))
            return;
        const auto& geometry = with<resource::geometry::Runtime>::get(context, grid->geometry);
        const auto limited = patches.size() > static_cast<std::size_t>(grid->patchCapacity) ? patches.first(static_cast<std::size_t>(grid->patchCapacity)) : patches;
        const auto packed = packTiles(limited);
        glfwMakeContextCurrent(with<system::Device>::get(context, grid->device).handle);
        if (not packed.empty())
            glNamedBufferSubData(grid->patches, 0, static_cast<renderer::SizePtr>(packed.size() * sizeof(GpuTile)), packed.data());
        const renderer::DrawElementsIndirect command{
            .count = static_cast<renderer::Integer32>(geometry.index_count),
            .instanceCount = static_cast<renderer::Integer32>(packed.size()),
            .firstIndex = renderer::Integer32{0},
            .baseVertex = renderer::Signed32{0},
            .baseInstance = renderer::Integer32{0},
        };
        glNamedBufferSubData(grid->indirect, 0, sizeof(command), &command);
        grid->patchCount = static_cast<integer>(packed.size());
    }

    void PatchGrid::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& grid = with<PatchGrid>::get(context, node);
        if (not with<Node>::get(context, node).visible or grid.device != device) return;
        const auto state = fieldState(context, node, grid);
        glNamedBufferSubData(grid.actorState, 0, sizeof(FieldState), &state);
        const auto& material = with<resource::material::Runtime>::get(context, grid.material);
        for (const auto& [pass, technique] : material.techniques)
            where.gpu[pass].push_back(gpuBatch(grid, grid.material, technique.shader, material.renderState));
    }

    struct PatchGrid::Internals : PatchGrid::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            if (not with<system::Device>::exists(context, last.device)) return;
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            deleteBuffers(last);
        }
    };

    auto PatchGrid::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<PatchGrid>(&PatchGrid::Internals::release),
        };
    }

}
