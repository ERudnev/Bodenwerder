#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/submit.h>

#include <rmmr/resources/runtimes.q1.h>

#include <glm/gtc/matrix_transform.hpp>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    namespace {

        auto albedo_layer_of(const resource::material::Instance& instance) -> base::maybe<string> {
            const auto it = instance.textures.find("albedoMap");
            if (it == instance.textures.end()) {
                return {};
            }
            return it->second;
        }

    } // namespace

    auto Mesh::Actions::create(Writing context, Pos position, HPB hpb, resource::geometry::Asset::Id geometry, umap<string, resource::material::Instance> parts, base::maybe<resource::texpack::Pack::Id> texpack, RGB albedo) -> Id {
        const auto node = Node::Actions::create(context, Pose::from(position, hpb));
        with<Mesh>::extend(context, node, Mesh::Quantum{
            .geometry = geometry,
            .parts = std::move(parts),
            .texpack = texpack,
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
        for (const auto& [part_name, instance] : actor.parts) {
            const auto part_it = geometry.parts.find(part_name);
            if (part_it == geometry.parts.end()) {
                continue;
            }
            drew_part = true;
            submit_material_passes(context, device, DrawInstance{
                .model = model,
                .geometry = actor.geometry,
                .material = instance.material,
                .texpack = actor.texpack,
                .albedoLayer = albedo_layer_of(instance),
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
        if (not drew_part and not actor.parts.empty()) {
            const auto& instance = actor.parts.begin()->second;
            submit_material_passes(context, device, DrawInstance{
                .model = model,
                .geometry = actor.geometry,
                .material = instance.material,
                .texpack = actor.texpack,
                .albedoLayer = albedo_layer_of(instance),
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
        Identified::BaseActions::extend(context, mesh, Identified::Quantum{.scenicAlias = alias, .selected = false});
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

    void Identified::Actions::applySelection(Writing context, const vector<renderer::Integer32>& aliases) {
        for (const auto [id, _] : context->aspect<Identified>().items())
            with<Identified>::modify(context, id)->selected = false;
        for (const auto alias : aliases) {
            if (const auto id = lookup(context, alias))
                with<Identified>::modify(context, *id)->selected = true;
        }
    }

    void Identified::Actions::submit(Reading context, Id node, system::Device::Id device, renderer::CommandBuffer& where) {
        const auto& mesh = with<Mesh>::get(context, node);
        if (not mesh.visible)
            return;
        if (not with<resource::geometry::Asset>::exists(context, mesh.geometry))
            return;

        const auto& identified = with<Identified>::get(context, node);
        auto model = Node::Actions::transform(context, node);
        model = glm::scale(model, mesh.scale);
        submit_identity(context, device, DrawInstance::Identiffy{
            .model = model,
            .geometry = mesh.geometry,
            .scenicAlias = identified.scenicAlias,
            .selected = identified.selected,
        }, where);
    }

}
