#pragma once

#include <base/maybe.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    // Instance + assets; techniques keys select baskets in CommandBuffer.
    struct DrawInstance {
        struct IndexRange {
            renderer::Count start;
            renderer::Count count;
        };

        struct Sprite {
            resource::sprite::Pack::Id pack;
            integer index;
        };

        // Identity pick: full geometry, one scenicAlias; material from Identified::Global.
        // selected → also queued into Pass::identitySelected (same alias value).
        struct Identiffy {
            mat4 model;
            resource::geometry::Asset::Id geometry;
            renderer::Integer32 scenicAlias;
            bool selected;
        };

        mat4 model;
        resource::geometry::Asset::Id geometry;
        resource::material::Asset::Id material;
        base::maybe<Sprite> sprite;
        RGB albedo;
        float opacity;
        float pattern_scale;
        renderer::Integer32 scenicAlias;
        base::maybe<IndexRange> indices;
    };

    void submit_material_passes(Reading, system::Device::Id, const DrawInstance&, renderer::CommandBuffer& where);

    // Push only Pass::identity; binds shared Identified::Global.material; full index range.
    void submit_identity(Reading, system::Device::Id, const DrawInstance::Identiffy&, renderer::CommandBuffer& where);

}
