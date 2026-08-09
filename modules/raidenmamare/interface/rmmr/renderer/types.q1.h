#pragma once

#include <base/maybe.h>

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/semantics/rendering.h>

#include <fQSM/api/interface.h>

#include <cstddef>

namespace rmmr::renderer {

    using namespace fqsm::api;

    struct RenderState {
        BlendMode blend;
    };

    struct DiscretePose {
        index3 pos;
        Signed32 ori;

        static auto identity() -> DiscretePose { return DiscretePose{.pos = index3{0, 0, 0}, .ori = Signed32{0}}; }
    };

    struct DrawElementsIndirect {
        Integer32 count;
        Integer32 instanceCount;
        Integer32 firstIndex;
        Signed32 baseVertex;
        Integer32 baseInstance;
    };

    // gl_DrawID selects metadata; gl_BaseInstance selects DiscretePose.
    // Surface layer = surfacePalette[surfaceBase + primitiveSurfaces[primitiveBase + gl_PrimitiveID]].
    struct DrawMetadata {
        Integer32 primitiveBase;
        Integer32 surfaceBase;
    };

    static_assert(sizeof(DiscretePose) == 16);
    static_assert(offsetof(DiscretePose, ori) == 12);
    static_assert(sizeof(DrawElementsIndirect) == 20);
    static_assert(sizeof(DrawMetadata) == 8);

    struct ActorState {
        mat4 model;
        vec4 albedoOpacity;
        vec2 latticePattern;
        Integer32 scenicAlias;
        Integer32 spriteIndex;
    };

    struct PassState {
        mat4 view;
        mat4 projection;
        mat4 lightSpace;
        vec4 ambientColorIntensity;
        vec4 primaryLightPositionIntensity;
        vec4 primaryLightColorRange;
    };

    static_assert(sizeof(ActorState) == 96);
    static_assert(offsetof(ActorState, albedoOpacity) == 64);
    static_assert(offsetof(ActorState, latticePattern) == 80);
    static_assert(offsetof(ActorState, scenicAlias) == 88);
    static_assert(offsetof(ActorState, spriteIndex) == 92);
    static_assert(sizeof(PassState) == 240);

    namespace StorageBindings {

        inline constexpr Integer32 actorState = 7;
        inline constexpr Integer32 poses = 8;
        inline constexpr Integer32 drawMetadata = 9;
        inline constexpr Integer32 surfacePalette = 10;
        inline constexpr Integer32 primitiveSurfaces = 11;

    }

    struct GpuBatch {
        resource::geometry::Runtime::Id geometry;
        resource::material::Runtime::Id material;
        resource::shader::Runtime::Id shader;
        base::maybe<resource::texpack::Runtime::Id> texpack;
        base::maybe<resource::sprite::Runtime::Id> sprite;
        StorageBuffer actorState;
        StorageBuffer poses;
        StorageBuffer drawMetadata;
        StorageBuffer surfacePalette;
        IntPtr metadataByteOffset;
        SizePtr metadataByteSize;
        IndirectBuffer indirect;
        Count drawCount;
        RenderState renderState;
    };

    struct CommandBuffer {
        SeparateBuffers<GpuBatch> gpu;
    };

}
