#include "renderer.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <rmmr/resources_old/geometry.q1.h>
#include <rmmr/resources_old/material.q1.h>
#include <rmmr/resources_old/shader.q1.h>
#include <rmmr/resources_old/texture.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources_old/shadowMap.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace rmmr {

    using namespace fqsm::api;
    using namespace api_for_internals;

    namespace {

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

        void set_uniform_sampler(const asset::Uniform::Binding& binding, GLuint texture, GLint unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(binding.location, unit);
        }

        auto material_texture_for_semantic(const resource_old::Material::Quantum& material, asset::Uniform::Id semantic) -> base::maybe<resource_old::Texture::Id> {
            for (const auto& texture_binding : material.textures) {
                if (texture_binding.id == semantic) {
                    return texture_binding.texture;
                }
            }
            return {};
        }

        auto light_space_matrix(Reading context, scene::Root::Id root) -> mat4 {
            const auto light_node = first_light_node(context, root);
            const mat4 light_transform = scene::Node::Actions::transform(context, light_node);
            const glm::vec3 light_position{light_transform[3]};
            const glm::vec3 scene_center{0.0f, 0.0f, 0.0f};
            const mat4 light_view = glm::lookAt(light_position, scene_center, glm::vec3{0.0f, 1.0f, 0.0f});
            const mat4 light_projection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
            return light_projection * light_view;
        }

        void begin_pass(renderer::Pass pass, Renderer::FrameContext args) {
            if (pass == renderer::Pass::shadow) {
                resource_old::ShadowMap::Actions::bind(args.world, args.shadow_map);
                resource_old::ShadowMap::Actions::clear(args.world, args.shadow_map);
                return;
            }

            if (pass == renderer::Pass::transparent || pass == renderer::Pass::gizmo) {
                glDepthMask(GL_FALSE);
            } else {
                glDepthMask(GL_TRUE);
            }
        }

        void end_pass(renderer::Pass pass, Renderer::FrameContext args) {
            if (pass == renderer::Pass::shadow) {
                resource_old::ShadowMap::Actions::unbind(args.world, args.shadow_map);
                system::Viewport::Actions::activate(args.world, args.viewport);
            }
        }

        constexpr std::array<renderer::Pass, 4> render_queue_passes{
            renderer::Pass::shadow,
            renderer::Pass::opaque,
            renderer::Pass::transparent,
            renderer::Pass::gizmo,
        };

    } // namespace

    void Renderer::ensure_material(FrameContext args, renderer::Pass pass, resource_old::Material::Id material, PassDrawState& state) {
        if (state.bound_material && *state.bound_material == material) {
            return;
        }

        with<resource_old::Material>::apply(args.world, material, args.window);
        bind_pass_uniforms(args, pass, material);
        state.bound_material = material;
        state.bound_geometry.reset();
    }

    void Renderer::bind_pass_uniforms(FrameContext args, renderer::Pass pass, resource_old::Material::Id material) {
        const auto& material_quantum = with<resource_old::Material>::get(args.world, material);

        if (pass == renderer::Pass::shadow) {
            const mat4 light_space = light_space_matrix(args.world, args.scene);
            for (const auto& binding : material_quantum.bindings) {
                if (binding.location < 0) {
                    continue;
                }
                if (material::Semantics::name_of(binding.id) == "lightSpaceMatrix") {
                    set_uniform(binding, light_space);
                }
            }
            return;
        }

        if (not with<scene::Camera>::exists(args.world, args.camera)) {
            throw std::runtime_error("Renderer: scene has no camera");
        }

        const auto& root_quantum = with<scene::Root>::get(args.world, args.scene);
        const float aspect_ratio = viewport_aspect_ratio(args.world, args.viewport);
        const mat4 view = scene::Camera::Actions::view(args.world, args.camera);
        const mat4 projection = scene::Camera::Actions::projection(args.world, args.camera, aspect_ratio);
        const mat4 light_space = light_space_matrix(args.world, args.scene);
        const auto& shadow_quantum = with<resource_old::ShadowMap>::get(args.world, args.shadow_map);

        const auto light_node = first_light_node(args.world, args.scene);
        const auto& light = with<scene::Light>::get(args.world, light_node);
        const mat4 light_transform = scene::Node::Actions::transform(args.world, light_node);
        const Pos light_world_pos{light_transform[3]};

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
            } else if (name == "lightSpaceMatrix") {
                set_uniform(binding, light_space);
            } else if (name == "shadowMap") {
                set_uniform_sampler(binding, shadow_quantum.depth, 1);
            } else if (name == "albedoMap") {
                const auto texture = material_texture_for_semantic(material_quantum, binding.id);
                if (not texture || not with<resource_old::Texture>::exists(args.world, *texture)) {
                    throw std::runtime_error("Renderer: material is missing albedoMap texture");
                }
                set_uniform_sampler(binding, with<resource_old::Texture>::get(args.world, *texture).handle, 0);
            } else if (name == "light0Pos") {
                set_uniform(binding, light_world_pos);
            } else if (name == "light0Color") {
                set_uniform(binding, light.color);
            } else if (name == "light0Intensity") {
                set_uniform(binding, light.intensity);
            }
        }
    }

    void Renderer::draw_instance(FrameContext args, const renderer::Command& command, resource_old::Material::Id material) {
        const auto& material_quantum = with<resource_old::Material>::get(args.world, material);

        for (const auto& binding : material_quantum.bindings) {
            if (binding.location < 0) {
                continue;
            }
            const auto name = material::Semantics::name_of(binding.id);
            if (name == "model") {
                set_uniform(binding, command.model);
            } else if (name == "albedo") {
                set_uniform(binding, command.albedo);
            } else if (name == "patternScale") {
                set_uniform(binding, 1.0f);
            } else if (name == "colorPrimary") {
                set_uniform(binding, RGB{0.45f, 0.48f, 0.52f} * command.opacity);
            } else if (name == "colorSecondary") {
                set_uniform(binding, RGB{0.1f, 0.12f, 0.14f} * command.opacity);
            }
        }
    }

    void Renderer::draw_stats_overlay(FrameContext args) {
        if (ImGui::Begin("Stats")) {
            const auto dt = with<system::Window>::dt(args.world, args.window);
            const auto fps = dt > 0.0 ? 1.0 / dt : 0.0;
            ImGui::Text("FPS: %.1f", fps);
            ImGui::Text("Frame time: %.3f ms", dt * 1000.0);
        }
        ImGui::End();
    }

    void Renderer::render(FrameContext args) {
        if (not with<scene::Camera>::exists(args.world, args.camera)) {
            base::message("Renderer: scene has no camera");
            return;
        }
        first_light_node(args.world, args.scene);

        renderer::CommandBuffer commands;
        scene::Interface::render(args.world, args.scene, commands);

        GLboolean depth_write_prev{};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_prev);

        for (const auto pass : render_queue_passes) {
            begin_pass(pass, args);
            PassDrawState pass_state{};

            for (const auto& command : commands) {
                if (command.pass != pass) {
                    continue;
                }
                if (command.instance_count <= renderer::Count{0}) {
                    continue;
                }
                if (not with<resource_old::Geometry>::exists(args.world, command.geometry)) {
                    continue;
                }

                ensure_material(args, pass, command.material, pass_state);

                const auto& geometry = with<resource_old::Geometry>::get(args.world, command.geometry);
                if (not pass_state.bound_geometry || *pass_state.bound_geometry != command.geometry) {
                    glBindVertexArray(geometry.vao);
                    pass_state.bound_geometry = command.geometry;
                }

                draw_instance(args, command, command.material);

                if (geometry.index_count > renderer::Count{0}) {
                    glDrawElements(GL_TRIANGLES, geometry.index_count, GL_UNSIGNED_INT, nullptr);
                } else {
                    glDrawArrays(GL_TRIANGLES, 0, geometry.vertex_count);
                }
            }

            end_pass(pass, args);
        }

        glDepthMask(depth_write_prev);
        draw_stats_overlay(args);
    }

}
