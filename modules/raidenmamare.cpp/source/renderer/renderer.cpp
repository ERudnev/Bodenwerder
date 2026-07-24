#include "renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <base/logging.h>
#include <base/maybe.h>

#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace rmmr {

    using namespace fqsm::api;
    using namespace api_for_internals;

    namespace {

        // Authoring names resolved once; draw/bind path compares PersistentId only.
        const struct {
            using Id = material::Semantics::PersistentId;
            Id model = material::Semantics::id_of("model");
            Id view = material::Semantics::id_of("view");
            Id projection = material::Semantics::id_of("projection");
            Id lightSpaceMatrix = material::Semantics::id_of("lightSpaceMatrix");
            Id albedo = material::Semantics::id_of("albedo");
            Id ambientColor = material::Semantics::id_of("ambientColor");
            Id ambientIntensity = material::Semantics::id_of("ambientIntensity");
            Id light0Color = material::Semantics::id_of("light0Color");
            Id light0Intensity = material::Semantics::id_of("light0Intensity");
            Id light0Pos = material::Semantics::id_of("light0Pos");
            Id patternScale = material::Semantics::id_of("patternScale");
            Id colorPrimary = material::Semantics::id_of("colorPrimary");
            Id colorSecondary = material::Semantics::id_of("colorSecondary");
            Id shadowMap = material::Semantics::id_of("shadowMap");
            Id albedoMap = material::Semantics::id_of("albedoMap");
        } semantic{};

        struct ShadowCaster {
            resource::shadow::Runtime::Id runtime;
        };

        struct FrameLighting {
            vector<scene::Light::Id> lights;
            base::maybe<ShadowCaster> shadow;
        };

        auto viewport_aspect_ratio(Reading context, system::Viewport::Id viewport) -> float {
            const auto& quantum = with<system::Viewport>::get(context, viewport);
            const float width = quantum.size.x > integer{0} ? static_cast<float>(quantum.size.x) : 1.0f;
            const float height = quantum.size.y > integer{0} ? static_cast<float>(quantum.size.y) : 1.0f;
            return width / height;
        }

        auto gather_lights(Reading context, scene::Root::Id root) -> vector<scene::Light::Id> {
            const auto& light_group = with<scene::Light_group>::get(context, root);
            return {light_group.begin(), light_group.end()};
        }

        // Empty lights → no shadow; Pass::shadow skipped in render(). Bind uses lights.front().
        auto assign_shadows_to_lights(Reading context, system::Device::Id device, vector<scene::Light::Id> lights) -> FrameLighting {
            FrameLighting lighting{
                .lights = std::move(lights),
                .shadow = {},
            };
            if (lighting.lights.empty())
                return lighting;

            const auto& runtimes = with<resource::Runtimes>::get(context, device);
            for (const auto& [_, runtime] : runtimes.shadows_id_mapping) {
                lighting.shadow = ShadowCaster{.runtime = runtime};
                break;
            }
            return lighting;
        }

        void set_uniform(const resource::Uniform::Binding& binding, const mat4& value) {
            glUniformMatrix4fv(binding.location, 1, GL_FALSE, glm::value_ptr(value));
        }

        void set_uniform(const resource::Uniform::Binding& binding, const vec3& value) {
            glUniform3f(binding.location, value.x, value.y, value.z);
        }

        void set_uniform(const resource::Uniform::Binding& binding, float value) {
            glUniform1f(binding.location, value);
        }

        void set_uniform_sampler(const resource::Uniform::Binding& binding, GLuint texture, GLint unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(binding.location, unit);
        }

        auto material_texture_for_semantic(const resource::material::Runtime::Technique& technique, resource::Uniform::Id semantic) -> base::maybe<resource::texture::Runtime::Id> {
            for (const auto& texture_binding : technique.textures) {
                if (texture_binding.uniform == semantic) {
                    return texture_binding.texture;
                }
            }
            return {};
        }

        auto technique_for(const resource::material::Runtime::Quantum& material, renderer::Pass pass) -> const resource::material::Runtime::Technique& {
            const auto it = material.techniques.find(pass);
            if (it == material.techniques.end()) {
                throw std::runtime_error("Renderer: material has no technique for pass");
            }
            return it->second;
        }

        auto light_space_matrix(Reading context, scene::Light::Id light_node) -> mat4 {
            const mat4 light_transform = scene::Node::Actions::transform(context, light_node);
            const glm::vec3 light_position{light_transform[3]};
            const glm::vec3 scene_center{0.0f, 0.0f, 0.0f};
            const mat4 light_view = glm::lookAt(light_position, scene_center, glm::vec3{0.0f, 1.0f, 0.0f});
            const mat4 light_projection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
            return light_projection * light_view;
        }

        void begin_pass(renderer::Pass pass, Renderer::FrameContext args, base::maybe<ShadowCaster> shadow) {
            if (pass == renderer::Pass::shadow) {
                resource::shadow::Runtime::Actions::bind(args.world, shadow->runtime);
                resource::shadow::Runtime::Actions::clear(args.world, shadow->runtime);
                return;
            }

            if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                return;
            }

            if (pass == renderer::Pass::gizmo) {
                glDepthMask(GL_FALSE);
            } else {
                glDepthMask(GL_TRUE);
            }
        }

        void end_pass(renderer::Pass pass, Renderer::FrameContext args, base::maybe<ShadowCaster> shadow) {
            if (pass == renderer::Pass::shadow) {
                resource::shadow::Runtime::Actions::unbind(args.world, shadow->runtime);
                system::Viewport::Actions::activate(args.world, args.viewport);
                return;
            }

            if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite) {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
        }

        constexpr std::array<renderer::Pass, 5> render_queue_passes{
            renderer::Pass::shadow,
            renderer::Pass::opaque,
            renderer::Pass::transparent,
            renderer::Pass::sprite,
            renderer::Pass::gizmo,
        };

        void sort_by_pipeline_state(renderer::Pass pass, renderer::CommandBuffer::Buffer& batch) {
            std::sort(batch.begin(), batch.end(), [pass](const renderer::Command& left, const renderer::Command& right) {
                if (left.shader != right.shader) {
                    return left.shader < right.shader;
                }
                // Shadow techniques are shared across surface materials: batch by mesh, not material.
                if (pass == renderer::Pass::shadow) {
                    return left.geometry < right.geometry;
                }
                if (left.material != right.material) {
                    return left.material < right.material;
                }
                return left.geometry < right.geometry;
            });
        }

        void sort_back_to_front(const mat4& view, renderer::CommandBuffer::Buffer& batch) {
            std::sort(batch.begin(), batch.end(), [&view](const renderer::Command& left, const renderer::Command& right) {
                const float left_depth = (view * left.model[3]).z;
                const float right_depth = (view * right.model[3]).z;
                return left_depth < right_depth;
            });
        }

    } // namespace

    void Renderer::ensure_material(
        FrameContext args,
        renderer::Pass pass,
        resource::material::Runtime::Id material,
        resource::shader::Runtime::Id shader,
        PassDrawState& state,
        base::maybe<scene::Light::Id> primary_light,
        base::maybe<resource::shadow::Runtime::Id> shadow)
    {
        // Depth-only shadow technique has no material-unique samplers: cache by program.
        if (pass == renderer::Pass::shadow) {
            if (state.bound_shader && *state.bound_shader == shader) {
                return;
            }
            with<resource::material::Runtime>::apply(args.world, material, args.window, pass);
            bind_pass_uniforms(args, pass, material, primary_light, shadow);
            state.bound_shader = shader;
            state.bound_material = material;
            state.bound_geometry.reset();
            return;
        }

        if (state.bound_material && *state.bound_material == material) {
            return;
        }

        const bool program_changed = not state.bound_shader || *state.bound_shader != shader;

        if (program_changed) {
            with<resource::material::Runtime>::apply(args.world, material, args.window, pass);
            bind_pass_uniforms(args, pass, material, primary_light, shadow);
            state.bound_shader = shader;
            state.bound_geometry.reset();
        } else {
            bind_material_samplers(args, pass, material);
        }

        state.bound_material = material;
    }

    void Renderer::bind_material_samplers(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material) {
        const auto& technique = technique_for(with<resource::material::Runtime>::get(args.world, material), pass);
        for (const auto& binding : technique.bindings) {
            if (binding.location < 0) {
                continue;
            }
            if (binding.id != semantic.albedoMap) {
                continue;
            }
            const auto texture = material_texture_for_semantic(technique, binding.id);
            if (not texture || not with<resource::texture::Runtime>::exists(args.world, *texture)) {
                throw std::runtime_error("Renderer: material is missing albedoMap texture");
            }
            set_uniform_sampler(binding, with<resource::texture::Runtime>::get(args.world, *texture).handle, 0);
        }
    }

    void Renderer::bind_pass_uniforms(
        FrameContext args,
        renderer::Pass pass,
        resource::material::Runtime::Id material,
        base::maybe<scene::Light::Id> primary_light,
        base::maybe<resource::shadow::Runtime::Id> shadow)
    {
        const auto& technique = technique_for(with<resource::material::Runtime>::get(args.world, material), pass);

        if (pass == renderer::Pass::shadow) {
            if (not primary_light) {
                throw std::runtime_error("Renderer: shadow pass requires a light");
            }
            const mat4 light_space = light_space_matrix(args.world, *primary_light);
            for (const auto& binding : technique.bindings) {
                if (binding.location < 0) {
                    continue;
                }
                if (binding.id == semantic.lightSpaceMatrix) {
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

        base::maybe<mat4> light_space{};
        base::maybe<Pos> light_world_pos{};
        base::maybe<scene::Light::Quantum> light{};
        if (primary_light) {
            light_space = light_space_matrix(args.world, *primary_light);
            light = with<scene::Light>::get(args.world, *primary_light);
            const mat4 light_transform = scene::Node::Actions::transform(args.world, *primary_light);
            light_world_pos = Pos{light_transform[3]};
        }

        for (const auto& binding : technique.bindings) {
            if (binding.location < 0) {
                continue;
            }
            if (binding.id == semantic.ambientColor) {
                set_uniform(binding, root_quantum.ambient);
            } else if (binding.id == semantic.ambientIntensity) {
                set_uniform(binding, root_quantum.ambient_intensity);
            } else if (binding.id == semantic.view) {
                set_uniform(binding, view);
            } else if (binding.id == semantic.projection) {
                set_uniform(binding, projection);
            } else if (binding.id == semantic.lightSpaceMatrix) {
                if (not light_space) {
                    throw std::runtime_error("Renderer: material expects lightSpaceMatrix but no light");
                }
                set_uniform(binding, *light_space);
            } else if (binding.id == semantic.shadowMap) {
                if (not shadow) {
                    throw std::runtime_error("Renderer: material expects shadowMap but no shadow-casting light");
                }
                set_uniform_sampler(binding, with<resource::shadow::Runtime>::get(args.world, *shadow).depth, 1);
            } else if (binding.id == semantic.albedoMap) {
                const auto texture = material_texture_for_semantic(technique, binding.id);
                if (not texture || not with<resource::texture::Runtime>::exists(args.world, *texture)) {
                    throw std::runtime_error("Renderer: material is missing albedoMap texture");
                }
                set_uniform_sampler(binding, with<resource::texture::Runtime>::get(args.world, *texture).handle, 0);
            } else if (binding.id == semantic.light0Pos) {
                if (not light_world_pos) {
                    throw std::runtime_error("Renderer: material expects light0Pos but no light");
                }
                set_uniform(binding, *light_world_pos);
            } else if (binding.id == semantic.light0Color) {
                if (not light) {
                    throw std::runtime_error("Renderer: material expects light0Color but no light");
                }
                set_uniform(binding, light->color);
            } else if (binding.id == semantic.light0Intensity) {
                if (not light) {
                    throw std::runtime_error("Renderer: material expects light0Intensity but no light");
                }
                set_uniform(binding, light->intensity);
            }
        }
    }

    void Renderer::draw_instance(FrameContext args, renderer::Pass pass, const renderer::Command& command, resource::material::Runtime::Id material) {
        const auto& technique = technique_for(with<resource::material::Runtime>::get(args.world, material), pass);

        for (const auto& binding : technique.bindings) {
            if (binding.location < 0) {
                continue;
            }
            if (binding.id == semantic.model) {
                set_uniform(binding, command.model);
            } else if (binding.id == semantic.albedo) {
                set_uniform(binding, command.albedo);
            } else if (binding.id == semantic.patternScale) {
                set_uniform(binding, 1.0f);
            } else if (binding.id == semantic.colorPrimary) {
                set_uniform(binding, RGB{0.45f, 0.48f, 0.52f} * command.opacity);
            } else if (binding.id == semantic.colorSecondary) {
                set_uniform(binding, RGB{0.1f, 0.12f, 0.14f} * command.opacity);
            }
        }
    }

    void Renderer::render(FrameContext args) {
        if (not with<scene::Camera>::exists(args.world, args.camera)) {
            base::message("Renderer: scene has no camera");
            return;
        }

        const auto lights = gather_lights(args.world, args.scene);
        // Pipeline chunk: assign active shadow runtimes to lights (placeholder policy inside).
        const auto lighting = assign_shadows_to_lights(args.world, args.window, lights);
        base::maybe<resource::shadow::Runtime::Id> shadow{};
        if (lighting.shadow) {
            shadow = lighting.shadow->runtime;
        }

        renderer::CommandBuffer commands;
        scene::Interface::render(args.world, args.scene, args.window, commands);

        const mat4 view = scene::Camera::Actions::view(args.world, args.camera);

        GLboolean depth_write_prev{};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_prev);

        for (const auto pass : render_queue_passes) {
            if (pass == renderer::Pass::shadow && not lighting.shadow) {
                continue;
            }
            const bool unlit_pass = pass == renderer::Pass::sprite || pass == renderer::Pass::gizmo;
            if (lighting.lights.empty() && not unlit_pass) {
                if (not commands[pass].empty())
                    base::message("Renderer: no light; skipping draws for pass");
                continue;
            }

            begin_pass(pass, args, lighting.shadow);
            PassDrawState pass_state{};

            auto& batch = commands[pass];
            if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite) {
                sort_back_to_front(view, batch);
            } else {
                sort_by_pipeline_state(pass, batch);
            }

            base::maybe<scene::Light::Id> primary_light{};
            if (not lighting.lights.empty()) {
                primary_light = lighting.lights.front();
            }

            for (const auto& command : batch) {
                if (command.instance_count <= renderer::Count{0}) {
                    continue;
                }
                if (not with<resource::geometry::Runtime>::exists(args.world, command.geometry)) {
                    continue;
                }

                ensure_material(args, pass, command.material, command.shader, pass_state, primary_light, shadow);

                const auto& geometry = with<resource::geometry::Runtime>::get(args.world, command.geometry);
                if (not pass_state.bound_geometry || *pass_state.bound_geometry != command.geometry) {
                    glBindVertexArray(geometry.vao);
                    pass_state.bound_geometry = command.geometry;
                }

                draw_instance(args, pass, command, command.material);

                if (geometry.index_count > renderer::Count{0}) {
                    glDrawElements(GL_TRIANGLES, geometry.index_count, GL_UNSIGNED_INT, nullptr);
                } else {
                    glDrawArrays(GL_TRIANGLES, 0, geometry.vertex_count);
                }
            }

            end_pass(pass, args, lighting.shadow);
        }

        glDepthMask(depth_write_prev);
    }

}
