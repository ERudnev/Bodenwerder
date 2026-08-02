#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/submit.h>

#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    auto Mesh::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, umap<string, resource::material::Asset::Id> materials, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Locator{
            .pos = position,
            .euler = hpb,
        });
        with<Mesh>::extend(context, node, Mesh::Quantum{
            .geometry = geometry,
            .materials = std::move(materials),
            .albedo = albedo,
            .scale = vec3{1.0f, 1.0f, 1.0f},
        });
        return node;
    }

    void Mesh::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& actor = with<Mesh>::get(context, node);
        if (not with<resource::geometry::Asset>::exists(context, actor.geometry)) {
            return;
        }
        const auto& geometry = with<resource::geometry::Asset>::get(context, actor.geometry);
        auto model = Node::Actions::transform(context, node);
        model = glm::scale(model, actor.scale);

        for (const auto& [part_name, material_id] : actor.materials) {
            const auto part_it = geometry.parts.find(part_name);
            if (part_it == geometry.parts.end()) {
                continue;
            }
            submit_material_passes(context, device, DrawInstance{
                .model = model,
                .geometry = actor.geometry,
                .material = material_id,
                .albedo = actor.albedo,
                .opacity = 1.0f,
                .indices = DrawInstance::IndexRange{
                    .start = part_it->second.startIndex,
                    .count = part_it->second.countIndex,
                },
            }, where);
        }
    }

}
