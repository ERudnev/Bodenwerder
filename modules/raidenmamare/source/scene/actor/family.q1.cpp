#include <rmmr/scene/actors/family.q1.h>

#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace rmmr::scene::actor {

    using namespace fqsm::api;
    using Type = resource::Uniform::Type;

    namespace {

        auto typeBytes(Type type) -> std::size_t {
            switch (type) {
                case Type::f32:
                case Type::i32: return 4;
                case Type::v2f: return 8;
                case Type::v3f: return 12;
                case Type::m4f: return 64;
                case Type::sampler2d:
                case Type::sampler2dArray:
                case Type::sampler3d:
                case Type::ssbo: return 0;
            }
            return 0;
        }

        auto layoutOk(const Family::Layout& layout) -> bool {
            if (layout.instanceBytes < 0)
                return false;
            const auto bytes = static_cast<std::size_t>(layout.instanceBytes);
            for (const auto& field : layout.fields) {
                const auto width = typeBytes(field.type);
                if (width == 0 or field.offset < 0)
                    return false;
                if (static_cast<std::size_t>(field.offset) + width > bytes)
                    return false;
            }
            return true;
        }

        auto findField(const Family::Layout& layout, const string& name) -> const Family::Field* {
            for (const auto& field : layout.fields) {
                if (field.name == name)
                    return &field;
            }
            return nullptr;
        }

        template<typename T>
        void writeValue(const Family::Layout& layout, Packed& packed, const string& name, Type expected, const T& value) {
            if (packed.size() != static_cast<std::size_t>(layout.instanceBytes))
                throw std::runtime_error("scene::actor::Family::write: packed size is not Family.layout.instanceBytes");
            const auto* field = findField(layout, name);
            if (not field)
                throw std::runtime_error("scene::actor::Family::write: unknown field '" + name + "'");
            if (field->type != expected)
                throw std::runtime_error("scene::actor::Family::write: type mismatch for '" + name + "'");
            const auto width = typeBytes(field->type);
            if (width != sizeof(T))
                throw std::runtime_error("scene::actor::Family::write: value size mismatch for '" + name + "'");
            std::memcpy(packed.data() + field->offset, &value, width);
        }

        auto bucketFromMesh(const Mesh::Bucket& mesh) -> Family::Bucket {
            return Family::Bucket{
                .geometry = mesh.geometry,
                .material = mesh.material,
                .texpack = mesh.texpack,
                .indirect = mesh.indirect,
                .drawCount = mesh.drawCount,
                .metadataByteOffset = mesh.metadataByteOffset,
                .metadataByteSize = mesh.metadataByteSize,
            };
        }

        void makeIndirectDynamic(Family::Bucket& bucket) {
            if (bucket.drawCount <= renderer::Count{0} or not bucket.indirect)
                return;
            vector<renderer::DrawElementsIndirect> commands(static_cast<std::size_t>(bucket.drawCount));
            glGetNamedBufferSubData(bucket.indirect, 0, static_cast<GLsizeiptr>(commands.size() * sizeof(renderer::DrawElementsIndirect)), commands.data());
            glDeleteBuffers(1, &bucket.indirect);
            bucket.indirect = renderer::IndirectBuffer{0};
            glCreateBuffers(1, &bucket.indirect);
            for (auto& command : commands) {
                command.instanceCount = renderer::Integer32{1};
                command.baseInstance = renderer::Integer32{0};
            }
            glNamedBufferStorage(bucket.indirect, static_cast<renderer::SizePtr>(commands.size() * sizeof(renderer::DrawElementsIndirect)), commands.data(), GL_DYNAMIC_STORAGE_BIT);
        }

        void deleteFamilyBuffers(const Family::Quantum& family);

        auto stealMeshAsFamily(Reading context, Mesh::Quantum mesh, Family::Layout layout) -> optional<Family::Quantum> {
            glfwMakeContextCurrent(with<system::Device>::get(context, mesh.device).handle);
            if (mesh.poses) {
                auto poses = mesh.poses;
                glDeleteBuffers(1, &poses);
                mesh.poses = renderer::StorageBuffer{0};
            }
            renderer::StorageBuffer instances{0};
            glCreateBuffers(1, &instances);
            if (not instances) {
                for (auto& meshBucket : mesh.buckets) {
                    if (meshBucket.indirect) {
                        auto indirect = meshBucket.indirect;
                        glDeleteBuffers(1, &indirect);
                    }
                }
                deleteFamilyBuffers(Family::Quantum{.device = mesh.device, .layout = layout, .actorState = mesh.actorState, .instances = {}, .drawMetadata = mesh.drawMetadata, .surfacePalette = mesh.surfacePalette, .buckets = {}});
                return {};
            }
            glNamedBufferData(instances, static_cast<GLsizeiptr>(16 * sizeof(mat4)), nullptr, GL_DYNAMIC_DRAW);
            vector<Family::Bucket> buckets;
            buckets.reserve(mesh.buckets.size());
            for (auto& meshBucket : mesh.buckets) {
                auto bucket = bucketFromMesh(meshBucket);
                meshBucket.indirect = renderer::IndirectBuffer{0};
                makeIndirectDynamic(bucket);
                buckets.push_back(bucket);
            }
            return Family::Quantum{
                .device = mesh.device,
                .layout = std::move(layout),
                .actorState = mesh.actorState,
                .instances = instances,
                .drawMetadata = mesh.drawMetadata,
                .surfacePalette = mesh.surfacePalette,
                .buckets = std::move(buckets),
            };
        }

        void deleteFamilyBuffers(const Family::Quantum& family) {
            if (family.actorState) {
                auto buffer = family.actorState;
                glDeleteBuffers(1, &buffer);
            }
            if (family.instances) {
                auto buffer = family.instances;
                glDeleteBuffers(1, &buffer);
            }
            if (family.drawMetadata) {
                auto buffer = family.drawMetadata;
                glDeleteBuffers(1, &buffer);
            }
            if (family.surfacePalette) {
                auto buffer = family.surfacePalette;
                glDeleteBuffers(1, &buffer);
            }
            for (const auto& bucket : family.buckets) {
                if (bucket.indirect) {
                    auto indirect = bucket.indirect;
                    glDeleteBuffers(1, &indirect);
                }
            }
        }

        auto gpuBatch(const Family::Quantum& family, const Family::Bucket& bucket, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, base::maybe<resource::texpack::Runtime::Id> texpack, renderer::BlendMode blend) -> renderer::GpuBatch {
            return renderer::GpuBatch{
                .geometry = bucket.geometry,
                .material = material,
                .shader = shader,
                .texpack = texpack,
                .texture3array = {},
                .sprite = {},
                .actorState = family.actorState,
                .poses = family.instances,
                .drawMetadata = family.drawMetadata,
                .surfacePalette = family.surfacePalette,
                .metadataByteOffset = bucket.metadataByteOffset,
                .metadataByteSize = bucket.metadataByteSize,
                .indirect = bucket.indirect,
                .drawCount = bucket.drawCount,
                .renderState = renderer::RenderState{.blend = blend},
            };
        }

        void patchInstanceCount(const Family::Bucket& bucket, renderer::Integer32 live) {
            if (bucket.drawCount <= renderer::Count{0} or not bucket.indirect)
                return;
            vector<renderer::DrawElementsIndirect> commands(static_cast<std::size_t>(bucket.drawCount));
            glGetNamedBufferSubData(bucket.indirect, 0, static_cast<GLsizeiptr>(commands.size() * sizeof(renderer::DrawElementsIndirect)), commands.data());
            for (auto& command : commands) {
                command.instanceCount = live;
                command.baseInstance = renderer::Integer32{0};
            }
            glNamedBufferSubData(bucket.indirect, 0, static_cast<GLsizeiptr>(commands.size() * sizeof(renderer::DrawElementsIndirect)), commands.data());
        }

    } // namespace

    auto Family::Actions::field(Reading context, Id id, string name) -> optional<Field> {
        const auto* found = findField(with<Family>::get(context, id).layout, name);
        if (not found)
            return {};
        return *found;
    }

    void Family::Actions::write(Reading context, Id id, Packed& packed, string name, float value) {
        writeValue(with<Family>::get(context, id).layout, packed, name, Type::f32, value);
    }

    void Family::Actions::write(Reading context, Id id, Packed& packed, string name, integer value) {
        const auto packed32 = static_cast<std::int32_t>(value);
        writeValue(with<Family>::get(context, id).layout, packed, name, Type::i32, packed32);
    }

    void Family::Actions::write(Reading context, Id id, Packed& packed, string name, vec2 value) {
        writeValue(with<Family>::get(context, id).layout, packed, name, Type::v2f, value);
    }

    void Family::Actions::write(Reading context, Id id, Packed& packed, string name, vec3 value) {
        writeValue(with<Family>::get(context, id).layout, packed, name, Type::v3f, value);
    }

    auto Family::Actions::compose(Reading context, resource::meshpack::Asset::Resolved resolved, Layout layout) -> optional<Quantum> {
        if (not layoutOk(layout))
            return {};
        auto mesh = Mesh::Actions::compose(context, std::move(resolved));
        if (not mesh)
            return {};
        return stealMeshAsFamily(context, std::move(*mesh), std::move(layout));
    }

    auto Family::Actions::composeOne(Reading context, resource::geometry::Asset::Id geometryId, resource::material::Asset::Id material, Layout layout) -> optional<Quantum> {
        if (not layoutOk(layout))
            return {};
        auto mesh = Mesh::Actions::composeOne(context, geometryId, material);
        if (not mesh)
            return {};
        return stealMeshAsFamily(context, std::move(*mesh), std::move(layout));
    }

    void Family::Actions::submit(Reading context, Id id, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& family = with<Family>::get(context, id);
        if (family.device != device or family.buckets.empty())
            return;
        vector<mat4> models;
        if (with<Replica_group>::exists(context, id)) {
            for (const auto replica : with<Replica_group>::get(context, id)) {
                if (not with<Replica>::exists(context, replica) or not with<Node>::exists(context, replica) or not with<Node>::get(context, replica).visible)
                    continue;
                models.push_back(Node::Actions::transform(context, replica));
            }
        }
        if (models.empty())
            return;
        glfwMakeContextCurrent(with<system::Device>::get(context, device).handle);
        glNamedBufferData(family.instances, static_cast<GLsizeiptr>(models.size() * sizeof(mat4)), models.data(), GL_DYNAMIC_DRAW);
        const auto live = static_cast<renderer::Integer32>(models.size());
        const renderer::ActorState gpuState{
            .model = mat4{1.0f},
            .albedoOpacity = vec4{1.0f, 1.0f, 1.0f, 1.0f},
            .latticePattern = vec2{1.0f, 1.0f},
            .scenicAlias = renderer::Integer32{0},
            .spriteIndex = renderer::Integer32{0},
            .heat = vec4{0.0f, 1.0f, 0.0f, 0.0f},
        };
        glNamedBufferSubData(family.actorState, 0, sizeof(renderer::ActorState), &gpuState);
        for (const auto& bucket : family.buckets) {
            patchInstanceCount(bucket, live);
            const auto& material = with<resource::material::Runtime>::get(context, bucket.material);
            for (const auto& [pass, technique] : material.techniques) {
                where.gpu[pass].push_back(gpuBatch(family, bucket, bucket.material, technique.shader, bucket.texpack, material.blend));
            }
        }
    }

    struct Family::Internals : Family::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            if (not with<system::Device>::exists(context, last.device))
                return;
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            deleteFamilyBuffers(last);
        }
    };

    auto Family::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Family>(&Family::Internals::release),
        };
    }

}
