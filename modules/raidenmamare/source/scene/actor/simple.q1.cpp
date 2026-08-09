#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/submit.h>

#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    auto Simple::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, resource::material::Asset::Id material, base::maybe<resource::texpack::Pack::Id> texpack, base::maybe<string> albedoLayer, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Pose::from(position, hpb));
        with<Simple>::extend(context, node, Simple::Quantum{
            .geometry = geometry,
            .material = material,
            .texpack = texpack,
            .albedoLayer = std::move(albedoLayer),
            .albedo = albedo,
            .scale = vec3{1.0f, 1.0f, 1.0f},
        });
        return node;
    }

    void Simple::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& actor = with<Simple>::get(context, node);
        auto model = Node::Actions::transform(context, node);
        model = glm::scale(model, actor.scale);
        submit_material_passes(context, device, DrawInstance{
            .model = model,
            .geometry = actor.geometry,
            .material = actor.material,
            .texpack = actor.texpack,
            .albedoLayer = actor.albedoLayer,
            .albedo = actor.albedo,
            .opacity = 1.0f,
            .pattern_scale = 1.0f,
            .scenicAlias = renderer::Integer32{0},
        }, where);
    }

}
