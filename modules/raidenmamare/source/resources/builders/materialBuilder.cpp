#include <rmmr/resources/builders/materialBuilder.h>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <utility>

namespace rmmr::resource::builders::material {

    using resource::material::Asset;

    auto addSinglePass(Writing context, SinglePass spec) -> Asset::Id {
        const auto shaderName = spec.name;
        const auto shaderAsset = with<Assets>::add_shader_loader(context, shaderName, std::move(spec.shader));
        return with<Assets>::add_material(context, std::move(spec.name), Asset::Quantum{
            .techniques = {{spec.pass, Asset::Technique{.program = with<Unit>::remember(context, shaderAsset), .uniforms = std::move(spec.uniforms), .glowSpread = spec.glowSpread, .lighting = spec.lighting}}},
            .nearest = spec.nearest,
            .renderState = spec.renderState,
        });
    }

    auto derive(Writing context, Derived spec) -> base::maybe<Asset::Id> {
        auto material = with<Asset>::get(context, spec.source);
        const auto source = material.techniques.find(spec.sourcePass);
        if (source == material.techniques.end())
            return context.refuse("rmmr::resource::builders::material::derive: source pass missing");
        auto technique = source->second;
        material.techniques.erase(source);
        if (spec.program)
            technique.program = *spec.program;
        technique.glowSpread = spec.glowSpread;
        material.techniques.insert_or_assign(spec.targetPass, std::move(technique));
        if (spec.renderState)
            material.renderState = *spec.renderState;
        return with<Assets>::add_material(context, std::move(spec.name), std::move(material));
    }

}
