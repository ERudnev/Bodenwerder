#include <rmmr/scene/actors/simple.q1.h>
#include <rmmr/scene/submit.h>

#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    auto Simple::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, resource::material::Asset::Id material, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Locator{
            .pos = position,
            .euler = hpb,
        });
        with<Simple>::extend(context, node, Simple::Quantum{
            .geometry = geometry,
            .material = material,
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
            .albedo = actor.albedo,
            .opacity = 1.0f,
        }, where);
    }

}
