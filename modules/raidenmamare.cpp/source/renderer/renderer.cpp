#include "renderer.h"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>

#include <Raidenmamare/materials/core.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/scene/actor.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>
#include <Raidenmamare/scene/node.q1.h>

namespace rmmr {

    static auto viewport_aspect_ratio(Reading world, Viewport::Id viewport) -> float {
        const auto& quantum = ops::particle::get<Viewport>(world, viewport);
        const float width = quantum.size.x > integer{0} ? static_cast<float>(quantum.size.x) : 1.0f;
        const float height = quantum.size.y > integer{0} ? static_cast<float>(quantum.size.y) : 1.0f;
        return width / height;
    }

    void Renderer::bind_material(PassArguments args, material::Core::RuntimeAccess material) {
        const auto& scene = ops::particle::get<scene::Core>(args.world, args.scene);
        glUseProgram(material.program);

        if (scene.cameras.empty()) { throw std::runtime_error("scene has no camera"); }
        const auto camera = scene.cameras.front();
        const float aspect_ratio = viewport_aspect_ratio(args.world, args.viewport);
        const mat4 view = scene::Camera::Operations::view(args.world, camera);
        const mat4 projection = scene::Camera::Operations::projection(args.world, camera, aspect_ratio);

        for (const auto& binding : material.bindings) {
            if (binding.location < 0) { continue; }
            const auto name = material::Semantics::name_of(binding.id);
            if (name == "ambientColor") {
                glUniform3f(binding.location, scene.ambient.x, scene.ambient.y, scene.ambient.z);
            } else if (name == "ambientIntensity") {
                glUniform1f(binding.location, scene.ambient_intensity);
            } else if (name == "view") {
                glUniformMatrix4fv(binding.location, 1, GL_FALSE, glm::value_ptr(view));
            } else if (name == "projection") {
                glUniformMatrix4fv(binding.location, 1, GL_FALSE, glm::value_ptr(projection));
            }
        }
    }

    void Renderer::bind_actor(PassArguments args, material::Core::RuntimeAccess material_runtime, scene::Node::Id node, const primitive::OpenGLPrimitive& primitive_runtime) {
        for (const auto& binding : material_runtime.bindings) {
            if (binding.location < 0) { continue; }
            const auto name = material::Semantics::name_of(binding.id);
            if (name == "model") {
                const mat4 model = scene::Node::Operations::transform(args.world, node);
                glUniformMatrix4fv(binding.location, 1, GL_FALSE, glm::value_ptr(model));
            }
        }
        glBindVertexArray(primitive_runtime.vao);
    }

    void Renderer::render_new_temp(PassArguments args) {
        const auto world = args.world;
        const auto viewport = args.viewport;
        const auto scene_id = args.scene;
        const resources::Manager manager{resources};

        const auto& scene_core = ops::particle::get<scene::Core>(world, scene_id);
        if (scene_core.cameras.empty()) { throw std::runtime_error("scene has no camera"); }
        if (scene_core.lights.empty()) { throw std::runtime_error("scene has no light"); }

        Viewport::Operations::activate(world, viewport, manager);
        Viewport::Operations::clear(world, viewport, manager);

        struct MaterialBatch {
            material::Core::Id material;
            vector<scene::Node::Id> nodes;
        };

        vector<MaterialBatch> batches;
        for (const auto node : scene_core.nodes) {
            if (!ops::particle::exists<scene::PrimitiveActor>(world, node)) { continue; }
            const auto& actor = ops::particle::get<scene::PrimitiveActor>(world, node);

            bool found = false;
            for (auto& batch : batches) {
                if (batch.material == actor.material) {
                    batch.nodes.push_back(node);
                    found = true;
                    break;
                }
            }
            if (!found) {
                batches.push_back(MaterialBatch{
                    .material = actor.material,
                    .nodes = {node},
                });
            }
        }

        for (const auto& batch : batches) {
            const auto& material_core_runtime = manager->layer<material::Core>().provide(batch.material);
            const auto shaderProgram = material_core_runtime.program;
            if (!shaderProgram) { throw std::runtime_error("actor material runtime program is null"); }

            const GLint albedo_location = glGetUniformLocation(shaderProgram, "u_albedo");

            bind_material(args, material_core_runtime);
            glUniform3f(albedo_location, 1.0f, 0.5f, 0.2f);

            for (const auto node : batch.nodes) {
                const auto& actor = ops::particle::get<scene::PrimitiveActor>(world, node);
                const auto& primitive_runtime = primitive::Base::Operations::provide(world, actor.geometry, manager);

                bind_actor(args, material_core_runtime, node, primitive_runtime);
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(primitive_runtime.vertex_count));
            }
        }
    }
} // namespace rmmr

