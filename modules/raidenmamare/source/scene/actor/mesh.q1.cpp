#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/submit.h>

#include <rmmr/resources/runtimes.q1.h>

#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    auto Mesh::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, umap<string, resource::material::Asset::Id> materials, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Pose::from(position, hpb));
        with<Mesh>::extend(context, node, Mesh::Quantum{
            .geometry = geometry,
            .materials = std::move(materials),
            .albedo = albedo,
            .scale = vec3{1.0f, 1.0f, 1.0f},
            .opacity = 1.0f,
            .visible = true,
        });
        return node;
    }

    void Mesh::Actions::setVisible(Writing context, Id node, bool visible) {
        with<Mesh>::modify(context, node)->visible = visible;
    }

    void Mesh::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& actor = with<Mesh>::get(context, node);
        if (not actor.visible)
            return;
        if (not with<resource::geometry::Asset>::exists(context, actor.geometry)) {
            return;
        }
        const auto& geometry = with<resource::geometry::Asset>::get(context, actor.geometry);
        auto model = Node::Actions::transform(context, node);
        model = glm::scale(model, actor.scale);

        bool drew_part = false;
        for (const auto& [part_name, material_id] : actor.materials) {
            const auto part_it = geometry.parts.find(part_name);
            if (part_it == geometry.parts.end()) {
                continue;
            }
            drew_part = true;
            submit_material_passes(context, device, DrawInstance{
                .model = model,
                .geometry = actor.geometry,
                .material = material_id,
                .albedo = actor.albedo,
                .opacity = actor.opacity,
                .pattern_scale = 1.0f,
                .scenicAlias = renderer::Integer32{0},
                .indices = DrawInstance::IndexRange{
                    .start = part_it->second.startIndex,
                    .count = part_it->second.countIndex,
                },
            }, where);
        }
        // No matching parts (generator meshes, bootstrap placeholders): whole geometry, first material.
        if (not drew_part and not actor.materials.empty()) {
            submit_material_passes(context, device, DrawInstance{
                .model = model,
                .geometry = actor.geometry,
                .material = actor.materials.begin()->second,
                .albedo = actor.albedo,
                .opacity = actor.opacity,
                .pattern_scale = 1.0f,
                .scenicAlias = renderer::Integer32{0},
            }, where);
        }
    }

    void Identified::Actions::extend(Writing context, Mesh::Id mesh) {
        auto global = with<Identified>::modify_global(context);
        ++global->lastGeneratedId;
        const auto alias = static_cast<renderer::Integer32>(global->lastGeneratedId);
        Identified::BaseActions::extend(context, mesh, Identified::Quantum{.scenicAlias = alias});
    }

    auto Identified::Actions::lookup(Reading context, renderer::Integer32 alias) -> optional<Id> {
        if (alias == renderer::Integer32{0})
            return {};
        for (const auto [id, quantum] : context->aspect<Identified>().items()) {
            if (quantum.scenicAlias == alias)
                return id;
        }
        return {};
    }

    void Identified::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        // Same id as Mesh / Node — Feature guarantees host Mesh (and thus Node) exists.
        const auto& mesh = with<Mesh>::get(context, node);
        if (not mesh.visible)
            return;
        if (not with<resource::geometry::Asset>::exists(context, mesh.geometry))
            return;

        auto model = Node::Actions::transform(context, node);
        model = glm::scale(model, mesh.scale);
        // One draw, full IBO — all submeshes, one scenicAlias.
        submit_identity(context, device, DrawInstance::Identiffy{
            .model = model,
            .geometry = mesh.geometry,
            .scenicAlias = with<Identified>::get(context, node).scenicAlias,
        }, where);
    }

}
