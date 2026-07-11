#include "renderer.h"

#include <Raidenmamare/resources/geometry.q1.h>
#include <Raidenmamare/resources/material.q1.h>
#include <Raidenmamare/resources/shader.q1.h>
#include <Raidenmamare/scene/actor.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>
#include <Raidenmamare/scene/node.q1.h>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>

namespace rmmr {

    using namespace fqsm::api;
    using namespace api_for_internals;

    namespace {

        auto material_is_grid(Reading context, resource::Material::Id material) -> bool {
            const auto& quantum = with<resource::Material>::get(context, material);
            for (const auto& binding : quantum.bindings) {
                if (binding.location < 0) {
                    continue;
                }
                if (material::Semantics::name_of(binding.id) == "patternScale") {
                    return true;
                }
            }
            return false;
        }

        auto viewport_aspect_ratio(Reading context, system::Viewport::Id viewport) -> float {
            const auto& quantum = with<system::Viewport>::get(context, viewport);
            const float width = quantum.size.x > integer{0} ? static_cast<float>(quantum.size.x) : 1.0f;
            const float height = quantum.size.y > integer{0} ? static_cast<float>(quantum.size.y) : 1.0f;
            return width / height;
        }

        auto first_light_node(Reading context, scene::Root::Id root) -> scene::Light::Id {
            const auto& light_group = with<scene::Light_group>::get(context, root);
            if (light_group.empty()) {
                throw std::runtime_error("Renderer: scene has no light");
            }
            return *light_group.begin();
        }

        void set_uniform(const asset::Uniform::Binding& binding, const mat4& value) {
            glUniformMatrix4fv(binding.location, 1, GL_FALSE, glm::value_ptr(value));
        }

        void set_uniform(const asset::Uniform::Binding& binding, const vec3& value) {
            glUniform3f(binding.location, value.x, value.y, value.z);
        }

        void set_uniform(const asset::Uniform::Binding& binding, float value) {
            glUniform1f(binding.location, value);
        }

    } // namespace

    void Renderer::bind_material(PassArguments args, resource::Material::Id material) {
        const auto& root_quantum = with<scene::Root>::get(args.world, args.scene);

        with<resource::Material>::apply(args.world, material, args.window);

        if (not with<scene::Camera>::exists(args.world, args.camera)) {
            throw std::runtime_error("Renderer: scene has no camera");
        }

        const float aspect_ratio = viewport_aspect_ratio(args.world, args.viewport);
        const mat4 view = scene::Camera::Actions::view(args.world, args.camera);
        const mat4 projection = scene::Camera::Actions::projection(args.world, args.camera, aspect_ratio);
        const auto& material_quantum = with<resource::Material>::get(args.world, material);

        for (const auto& binding : material_quantum.bindings) {
            if (binding.location < 0) {
                continue;
            }
            const auto name = material::Semantics::name_of(binding.id);
            if (name == "ambientColor") {
                set_uniform(binding, root_quantum.ambient);
            } else if (name == "ambientIntensity") {
                set_uniform(binding, root_quantum.ambient_intensity);
            } else if (name == "view") {
                set_uniform(binding, view);
            } else if (name == "projection") {
                set_uniform(binding, projection);
            } else if (name == "patternScale") {
                set_uniform(binding, 1.0f);
            } else if (name == "colorPrimary") {
                set_uniform(binding, RGB{0.45f, 0.48f, 0.52f});
            } else if (name == "colorSecondary") {
                set_uniform(binding, RGB{0.1f, 0.12f, 0.14f});
            }
        }
    }

    void Renderer::bind_actor(
        PassArguments args,
        resource::Material::Id material,
        const scene::PrimitiveActor::Quantum& actor,
        scene::Node::Id node
    ) {
        const auto& geometry = with<resource::Geometry>::get(args.world, actor.geometry);
        const auto& material_quantum = with<resource::Material>::get(args.world, material);

        for (const auto& binding : material_quantum.bindings) {
            if (binding.location < 0) {
                continue;
            }
            const auto name = material::Semantics::name_of(binding.id);
            if (name == "model") {
                set_uniform(binding, scene::Node::Actions::transform(args.world, node));
            } else if (name == "albedo") {
                set_uniform(binding, actor.albedo);
            }
        }

        glBindVertexArray(geometry.vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(geometry.vertex_count));
    }

    void Renderer::bind_lights(PassArguments args, resource::Material::Id material) {
        const auto light_node = first_light_node(args.world, args.scene);
        const auto& light = with<scene::Light>::get(args.world, light_node);
        const mat4 light_transform = scene::Node::Actions::transform(args.world, light_node);
        const Pos light_world_pos{light_transform[3]};
        const auto& material_quantum = with<resource::Material>::get(args.world, material);

        for (const auto& binding : material_quantum.bindings) {
            if (binding.location < 0) {
                continue;
            }
            const auto name = material::Semantics::name_of(binding.id);
            if (name == "light0Pos") {
                set_uniform(binding, light_world_pos);
            } else if (name == "light0Color") {
                set_uniform(binding, light.color);
            } else if (name == "light0Intensity") {
                set_uniform(binding, light.intensity);
            }
        }
    }

    void Renderer::render(PassArguments args) {
        if (not with<scene::Camera>::exists(args.world, args.camera)) {
            throw std::runtime_error("Renderer: scene has no camera");
        }
        first_light_node(args.world, args.scene);

        struct MaterialBatch {
            resource::Material::Id material;
            vector<scene::Node::Id> nodes;
        };

        vector<MaterialBatch> batches;
        const auto& node_group = with<scene::Node_group>::get(args.world, args.scene);
        for (const auto node : node_group) {
            if (not with<scene::PrimitiveActor>::exists(args.world, node)) {
                continue;
            }
            const auto& actor = with<scene::PrimitiveActor>::get(args.world, node);

            bool found = false;
            for (auto& batch : batches) {
                if (batch.material == actor.material) {
                    batch.nodes.push_back(node);
                    found = true;
                    break;
                }
            }
            if (not found) {
                batches.push_back(MaterialBatch{
                    .material = actor.material,
                    .nodes = {node},
                });
            }
        }

        const auto draw_batch = [&](const MaterialBatch& batch) {
            const auto& shader = with<resource::Shader>::get(args.world, with<resource::Material>::get(args.world, batch.material).shader);
            if (not shader.handle) {
                throw std::runtime_error("Renderer: material shader program is null");
            }

            bind_material(args, batch.material);
            bind_lights(args, batch.material);

            for (const auto node : batch.nodes) {
                const auto& actor = with<scene::PrimitiveActor>::get(args.world, node);
                bind_actor(args, batch.material, actor, node);
            }
        };

        for (const auto& batch : batches) {
            if (material_is_grid(args.world, batch.material)) {
                continue;
            }
            draw_batch(batch);
        }

        GLboolean depth_write_prev{};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_prev);
        glDepthMask(GL_FALSE);

        for (const auto& batch : batches) {
            if (not material_is_grid(args.world, batch.material)) {
                continue;
            }
            draw_batch(batch);
        }

        glDepthMask(depth_write_prev);
    }

}
