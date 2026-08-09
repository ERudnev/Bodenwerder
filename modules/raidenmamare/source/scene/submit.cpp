#include <rmmr/scene/submit.h>

#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/semantics/uniform.h>

#include <stdexcept>

namespace rmmr::scene {

    using namespace fqsm::api;

    namespace {

        auto material_requires_albedo_map(Reading context, resource::material::Asset::Id material_id) -> bool {
            const auto& material = with<resource::material::Asset>::get(context, material_id);
            const auto albedo = ::rmmr::material::Semantics::id_of("albedoMap");
            for (const auto& [pass, technique] : material.techniques) {
                for (const auto uniform : technique.uniforms) {
                    if (uniform == albedo) {
                        return true;
                    }
                }
            }
            return false;
        }

        auto resolve_albedo_layer(
            Reading context,
            system::Device::Id device,
            base::maybe<resource::texpack::Pack::Id> texpack_id,
            base::maybe<string> layer_name)
            -> std::pair<base::maybe<resource::texpack::Runtime::Id>, base::maybe<integer>>
        {
            if (not texpack_id) {
                throw std::runtime_error("scene::submit: albedoMap required but texpack missing");
            }
            if (not layer_name or layer_name->empty()) {
                throw std::runtime_error("scene::submit: albedoMap required but layer name missing");
            }
            const auto& runtimes = with<resource::Runtimes>::get(context, device);
            const auto pack_it = runtimes.texpacks_id_mapping.find(*texpack_id);
            if (pack_it == runtimes.texpacks_id_mapping.end()) {
                throw std::runtime_error("scene::submit: texpack runtime missing");
            }
            const auto& pack_runtime = with<resource::texpack::Runtime>::get(context, pack_it->second);
            const auto layer_it = pack_runtime.layers.find(*layer_name);
            if (layer_it == pack_runtime.layers.end()) {
                throw std::runtime_error("scene::submit: albedo layer not in texpack: " + *layer_name);
            }
            return {pack_it->second, layer_it->second};
        }

    } // namespace

    void submit_material_passes(Reading context, system::Device::Id device, const DrawInstance& draw, renderer::CommandBuffer& where) {
        const auto& runtimes = with<resource::Runtimes>::get(context, device);

        const auto geometry_it = runtimes.geometries_id_mapping.find(draw.geometry);
        const auto material_it = runtimes.materials_id_mapping.find(draw.material);
        if (geometry_it == runtimes.geometries_id_mapping.end() || material_it == runtimes.materials_id_mapping.end()) {
            throw std::runtime_error("scene::submit: geometry or material runtime missing");
        }
        base::maybe<resource::sprite::Runtime::Id> sprite{};
        integer sprite_index = 0;
        if (draw.sprite) {
            const auto sprite_it = runtimes.sprites_id_mapping.find(draw.sprite->pack);
            if (sprite_it == runtimes.sprites_id_mapping.end()) {
                throw std::runtime_error("scene::submit: sprite runtime missing");
            }
            sprite = sprite_it->second;
            sprite_index = draw.sprite->index;
        }

        const auto& material = with<resource::material::Runtime>::get(context, material_it->second);

        base::maybe<resource::texpack::Runtime::Id> texpack{};
        base::maybe<integer> albedoLayer{};
        if (material_requires_albedo_map(context, draw.material)) {
            const auto resolved = resolve_albedo_layer(context, device, draw.texpack, draw.albedoLayer);
            texpack = resolved.first;
            albedoLayer = resolved.second;
        }

        for (const auto& [pass, technique] : material.techniques) {
            base::maybe<renderer::Command::IndexRange> indices{};
            if (draw.indices) {
                indices = renderer::Command::IndexRange{.start = draw.indices->start, .count = draw.indices->count};
            }
            where[pass].push_back(renderer::Command{
                .model = draw.model,
                .geometry = geometry_it->second,
                .material = material_it->second,
                .shader = technique.shader,
                .texpack = texpack,
                .albedoLayer = albedoLayer,
                .sprite = sprite,
                .sprite_index = sprite_index,
                .albedo = draw.albedo,
                .opacity = draw.opacity,
                .pattern_scale = draw.pattern_scale,
                .scenicAlias = draw.scenicAlias,
                .instance_data = {},
                .instance_count = renderer::Count{1},
                .render_state = renderer::RenderState{.blend = material.blend},
                .indices = indices,
            });
        }
    }

    void submit_identity(Reading context, system::Device::Id device, const DrawInstance::Identiffy& draw, renderer::CommandBuffer& where) {
        const auto& global = with<actor::Identified>::get_global(context);
        if (not global.material)
            return;

        const auto& runtimes = with<resource::Runtimes>::get(context, device);
        const auto geometry_it = runtimes.geometries_id_mapping.find(draw.geometry);
        const auto material_it = runtimes.materials_id_mapping.find(*global.material);
        if (geometry_it == runtimes.geometries_id_mapping.end() || material_it == runtimes.materials_id_mapping.end()) {
            throw std::runtime_error("scene::submit_identity: geometry or material runtime missing");
        }

        const auto& material = with<resource::material::Runtime>::get(context, material_it->second);
        const auto technique_it = material.techniques.find(renderer::Pass::identity);
        if (technique_it == material.techniques.end()) {
            throw std::runtime_error("scene::submit_identity: identity material missing Pass::identity");
        }

        const auto push = [&](renderer::Pass pass) {
            where[pass].push_back(renderer::Command{
                .model = draw.model,
                .geometry = geometry_it->second,
                .material = material_it->second,
                .shader = technique_it->second.shader,
                .texpack = {},
                .albedoLayer = {},
                .sprite = {},
                .sprite_index = 0,
                .albedo = RGB{1.0f, 1.0f, 1.0f},
                .opacity = 1.0f,
                .pattern_scale = 1.0f,
                .scenicAlias = draw.scenicAlias,
                .instance_data = {},
                .instance_count = renderer::Count{1},
                .render_state = renderer::RenderState{.blend = renderer::BlendMode::inherit},
                .indices = {},
            });
        };

        if (draw.selected)
            push(renderer::Pass::identitySelected);
        push(renderer::Pass::identity);
    }

}
