#pragma once

#include <base/maybe.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct DrawInstance {
        struct IndexRange {
            renderer::Count start;
            renderer::Count count;
        };

        struct Sprite {
            resource::sprite::Pack::Id pack;
            integer index;
        };

        struct Identiffy {
            mat4 model;
            resource::geometry::Asset::Id geometry;
            renderer::Integer32 scenicAlias;
            bool selected;
        };

        mat4 model;
        resource::geometry::Asset::Id geometry;
        resource::material::Asset::Id material;
        base::maybe<resource::texpack::Pack::Id> texpack;
        base::maybe<string> albedoLayer;
        base::maybe<Sprite> sprite;
        RGB albedo;
        float opacity;
        float pattern_scale;
        renderer::Integer32 scenicAlias;
        base::maybe<IndexRange> indices;
    };

    void submit_material_passes(Reading, system::Device::Id, const DrawInstance&, renderer::CommandBuffer& where);
    void submit_identity(Reading, system::Device::Id, const DrawInstance::Identiffy&, renderer::CommandBuffer& where);

}
