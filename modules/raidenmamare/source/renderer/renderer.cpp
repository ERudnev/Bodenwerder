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
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/system/viewport.q1.h>

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace rmmr {

    using namespace fqsm::api;
    using namespace api_for_internals;

    Renderer::Renderer()
        : identity_{.fbo = 0, .color = 0, .depth = 0, .size = index2{0, 0}}
    {}

    Renderer::~Renderer() {
        if (identity_.fbo)
            glDeleteFramebuffers(1, &identity_.fbo);
        if (identity_.color)
            glDeleteTextures(1, &identity_.color);
        if (identity_.depth)
            glDeleteTextures(1, &identity_.depth);
    }

    void Renderer::ensure_identity_target(index2 size) {
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        if (identity_.fbo and identity_.size.x == size.x and identity_.size.y == size.y)
            return;

        if (identity_.fbo) {
            glDeleteFramebuffers(1, &identity_.fbo);
            identity_.fbo = 0;
        }
        if (identity_.color) {
            glDeleteTextures(1, &identity_.color);
            identity_.color = 0;
        }
        if (identity_.depth) {
            glDeleteTextures(1, &identity_.depth);
            identity_.depth = 0;
        }

        glGenTextures(1, &identity_.color);
        glBindTexture(GL_TEXTURE_2D, identity_.color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenTextures(1, &identity_.depth);
        glBindTexture(GL_TEXTURE_2D, identity_.depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glGenFramebuffers(1, &identity_.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, identity_.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, identity_.color, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, identity_.depth, 0);
        const GLenum draw_buffers[]{GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            throw std::runtime_error("Renderer: identity framebuffer incomplete");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        identity_.size = size;
    }

    void Renderer::begin_identity_pass(index2 size) {
        ensure_identity_target(size);
        glBindFramebuffer(GL_FRAMEBUFFER, identity_.fbo);
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        glViewport(0, 0, width, height);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        const GLuint clear_alias[]{0u};
        glClearBufferuiv(GL_COLOR, 0, clear_alias);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void Renderer::end_identity_pass(FrameContext args) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        system::Viewport::Actions::activate(args.world, args.view.viewport);
    }

    auto Renderer::peek_identity_under(FrameContext args, index2 viewport_size) -> renderer::Integer32 {
        if (ImGui::GetIO().WantCaptureMouse)
            return renderer::Integer32{0};

        const auto& window = with<system::Window>::get(args.world, args.window);
        const auto& viewport = with<system::Viewport>::get(args.world, args.view.viewport);
        const auto fb = system::Window::Actions::framebufferSize(args.world, args.window);

        int win_w = 1;
        int win_h = 1;
        glfwGetWindowSize(with<system::Device>::get(args.world, args.window).handle, &win_w, &win_h);
        win_w = std::max(win_w, 1);
        win_h = std::max(win_h, 1);

        const auto fb_x = static_cast<integer>(std::lround(static_cast<double>(window.current.mouse.x) * static_cast<double>(fb.x) / static_cast<double>(win_w)));
        const auto fb_y = static_cast<integer>(std::lround(static_cast<double>(window.current.mouse.y) * static_cast<double>(fb.y) / static_cast<double>(win_h)));
        const auto local_x = fb_x - viewport.origin.x;
        const auto local_y = fb_y - viewport.origin.y;
        if (local_x < 0 or local_y < 0 or local_x >= viewport_size.x or local_y >= viewport_size.y)
            return renderer::Integer32{0};

        const int read_x = static_cast<int>(local_x);
        const int read_y = static_cast<int>(viewport_size.y) - 1 - static_cast<int>(local_y);
        GLuint alias = 0;
        glReadPixels(read_x, read_y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &alias);
        return static_cast<renderer::Integer32>(alias);
    }

    void Renderer::publish_identity(FrameContext args, integer draws, renderer::Integer32 under) {
        auto window = with<system::Window>::modify(args.world, args.window);
        window->identityDraws = draws;
        window->current.under = under;
    }

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
            Id atlasTexture = material::Semantics::id_of("atlasTexture");
            Id atlasEntries = material::Semantics::id_of("atlasEntries");
            Id spriteIndex = material::Semantics::id_of("spriteIndex");
            Id inverseAtlasSize = material::Semantics::id_of("inverseAtlasSize");
            Id opacity = material::Semantics::id_of("opacity");
            Id scenicAlias = material::Semantics::id_of("scenicAlias");
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
            const float width = quantum.size.x > 0 ? static_cast<float>(quantum.size.x) : 1.0f;
            const float height = quantum.size.y > 0 ? static_cast<float>(quantum.size.y) : 1.0f;
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

        void set_uniform(const resource::Uniform::Binding& binding, integer value) {
            glUniform1i(binding.location, value);
        }

        void set_uniform(const resource::Uniform::Binding& binding, const vec2& value) {
            glUniform2f(binding.location, value.x, value.y);
        }

        void set_uniform_sampler(const resource::Uniform::Binding& binding, GLuint texture, GLint unit, bool nearest = false) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture);
            if (nearest) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glUniform1i(binding.location, unit);
        }

        void set_uniform_sampler_buffer(const resource::Uniform::Binding& binding, GLuint texture, GLint unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_BUFFER, texture);
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

        auto sprite_runtime_for(Reading context, const renderer::Command& command) -> const resource::sprite::Runtime::Quantum& {
            if (not command.sprite || not with<resource::sprite::Runtime>::exists(context, *command.sprite)) {
                throw std::runtime_error("Renderer: sprite draw is missing sprite runtime");
            }
            return with<resource::sprite::Runtime>::get(context, *command.sprite);
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

            if (pass == renderer::Pass::environment) {
                // After clear, before opaque: fill backdrop without writing depth.
                glDepthMask(GL_FALSE);
                return;
            }

            if (pass == renderer::Pass::gizmo) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                return;
            }

            glDepthMask(GL_TRUE);
        }

        void end_pass(renderer::Pass pass, Renderer::FrameContext args, base::maybe<ShadowCaster> shadow) {
            if (pass == renderer::Pass::shadow) {
                resource::shadow::Runtime::Actions::unbind(args.world, shadow->runtime);
                system::Viewport::Actions::activate(args.world, args.view.viewport);
                return;
            }

            if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite || pass == renderer::Pass::environment || pass == renderer::Pass::gizmo) {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
        }

        // shadow is offscreen; identity is offscreen pick; environment is first color on the main FB…
        constexpr std::array<renderer::Pass, 7> render_queue_passes{
            renderer::Pass::shadow,
            renderer::Pass::environment,
            renderer::Pass::opaque,
            renderer::Pass::transparent,
            renderer::Pass::sprite,
            renderer::Pass::gizmo,
            renderer::Pass::identity,
        };

        void apply_blend(renderer::Pass pass, renderer::BlendMode blend) {
            if (blend == renderer::BlendMode::inherit) {
                if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite || pass == renderer::Pass::gizmo) {
                    blend = renderer::BlendMode::alpha;
                } else {
                    return;
                }
            }

            if (blend == renderer::BlendMode::alpha) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                return;
            }

            if (blend == renderer::BlendMode::additive) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                return;
            }

            if (blend == renderer::BlendMode::premultiplied) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            }
        }

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
        const auto& material_quantum = with<resource::material::Runtime>::get(args.world, material);
        const auto& technique = technique_for(material_quantum, pass);
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
            set_uniform_sampler(binding, with<resource::texture::Runtime>::get(args.world, *texture).handle, 0, material_quantum.nearest);
        }
    }

    void Renderer::bind_pass_uniforms(
        FrameContext args,
        renderer::Pass pass,
        resource::material::Runtime::Id material,
        base::maybe<scene::Light::Id> primary_light,
        base::maybe<resource::shadow::Runtime::Id> shadow)
    {
        const auto& material_quantum = with<resource::material::Runtime>::get(args.world, material);
        const auto& technique = technique_for(material_quantum, pass);

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

        if (not with<scene::Camera>::exists(args.world, args.view.camera)) {
            return;
        }

        const auto& root_quantum = with<scene::Root>::get(args.world, args.view.scene);
        const float aspect_ratio = viewport_aspect_ratio(args.world, args.view.viewport);
        const mat4 view = scene::Camera::Actions::view(args.world, args.view.camera);
        const mat4 projection = scene::Camera::Actions::projection(args.world, args.view.camera, aspect_ratio);

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
                set_uniform_sampler(binding, with<resource::texture::Runtime>::get(args.world, *texture).handle, 0, material_quantum.nearest);
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
        const auto& material_quantum = with<resource::material::Runtime>::get(args.world, material);
        const auto& technique = technique_for(material_quantum, pass);
        const resource::sprite::Runtime::Quantum* sprite = nullptr;
        if (command.sprite) {
            sprite = &sprite_runtime_for(args.world, command);
        }

        for (const auto& binding : technique.bindings) {
            if (binding.location < 0) {
                continue;
            }
            if (binding.id == semantic.model) {
                set_uniform(binding, command.model);
            } else if (binding.id == semantic.albedo) {
                set_uniform(binding, command.albedo);
            } else if (binding.id == semantic.opacity) {
                set_uniform(binding, command.opacity);
            } else if (binding.id == semantic.patternScale) {
                set_uniform(binding, command.pattern_scale);
            } else if (binding.id == semantic.colorPrimary) {
                set_uniform(binding, RGB{0.45f, 0.48f, 0.52f} * command.opacity);
            } else if (binding.id == semantic.colorSecondary) {
                set_uniform(binding, RGB{0.1f, 0.12f, 0.14f} * command.opacity);
            } else if (binding.id == semantic.atlasTexture) {
                if (not sprite) {
                    throw std::runtime_error("Renderer: atlasTexture requested on non-sprite draw");
                }
                const auto& texture = with<resource::texture::Runtime>::get(args.world, sprite->texture);
                set_uniform_sampler(binding, texture.handle, 0, material_quantum.nearest);
            } else if (binding.id == semantic.atlasEntries) {
                if (not sprite) {
                    throw std::runtime_error("Renderer: atlasEntries requested on non-sprite draw");
                }
                set_uniform_sampler_buffer(binding, sprite->entries_texture, 1);
            } else if (binding.id == semantic.spriteIndex) {
                set_uniform(binding, command.sprite_index);
            } else if (binding.id == semantic.scenicAlias) {
                set_uniform(binding, static_cast<integer>(command.scenicAlias));
            } else if (binding.id == semantic.inverseAtlasSize) {
                if (not sprite) {
                    throw std::runtime_error("Renderer: inverseAtlasSize requested on non-sprite draw");
                }
                const auto& texture = with<resource::texture::Runtime>::get(args.world, sprite->texture);
                const float inverse_width = texture.size.x > 0 ? 1.0f / static_cast<float>(texture.size.x) : 0.0f;
                const float inverse_height = texture.size.y > 0 ? 1.0f / static_cast<float>(texture.size.y) : 0.0f;
                set_uniform(binding, vec2{inverse_width, inverse_height});
            }
        }
    }

    void Renderer::render(FrameContext args) {
        if (not with<scene::Camera>::exists(args.world, args.view.camera)) {
            base::message("Renderer: scene has no camera");
            return;
        }

        const auto lights = gather_lights(args.world, args.view.scene);
        // Pipeline chunk: assign active shadow runtimes to lights (placeholder policy inside).
        const auto lighting = assign_shadows_to_lights(args.world, args.window, lights);
        base::maybe<resource::shadow::Runtime::Id> shadow{};
        if (lighting.shadow) {
            shadow = lighting.shadow->runtime;
        }

        renderer::CommandBuffer commands;
        scene::Interface::render(args.world, args.view.scene, args.window, commands);

        const mat4 view = scene::Camera::Actions::view(args.world, args.view.camera);

        GLboolean depth_write_prev{};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_prev);

        integer identity_draws = 0;
        renderer::Integer32 identity_under = renderer::Integer32{0};
        bool identity_published = false;

        for (const auto pass : render_queue_passes) {
            if (pass == renderer::Pass::shadow && not lighting.shadow) {
                continue;
            }
            if (pass == renderer::Pass::identity && commands[pass].empty()) {
                publish_identity(args, 0, renderer::Integer32{0});
                identity_published = true;
                continue;
            }
            const bool unlit_pass = pass == renderer::Pass::sprite
                || pass == renderer::Pass::gizmo
                || pass == renderer::Pass::environment
                || pass == renderer::Pass::identity;
            if (lighting.lights.empty() && not unlit_pass) {
                if (not commands[pass].empty())
                    base::message("Renderer: no light; skipping draws for pass");
                continue;
            }

            const auto& viewport = with<system::Viewport>::get(args.world, args.view.viewport);
            if (pass == renderer::Pass::identity) {
                begin_identity_pass(viewport.size);
            } else {
                begin_pass(pass, args, lighting.shadow);
            }
            PassDrawState pass_state{};

            auto& batch = commands[pass];
            if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite || pass == renderer::Pass::gizmo) {
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

                apply_blend(pass, command.render_state.blend);

                ensure_material(args, pass, command.material, command.shader, pass_state, primary_light, shadow);

                const auto& geometry = with<resource::geometry::Runtime>::get(args.world, command.geometry);
                if (not pass_state.bound_geometry || *pass_state.bound_geometry != command.geometry) {
                    glBindVertexArray(geometry.vao);
                    pass_state.bound_geometry = command.geometry;
                }

                draw_instance(args, pass, command, command.material);

                bool drew = false;
                if (geometry.index_count > renderer::Count{0}) {
                    if (command.indices) {
                        const auto& range = *command.indices;
                        if (range.count > renderer::Count{0}) {
                            const auto byte_offset = static_cast<renderer::IntPtr>(range.start) * static_cast<renderer::IntPtr>(sizeof(GLuint));
                            glDrawElements(GL_TRIANGLES, range.count, GL_UNSIGNED_INT, reinterpret_cast<const void*>(byte_offset));
                            drew = true;
                        }
                    } else {
                        glDrawElements(GL_TRIANGLES, geometry.index_count, GL_UNSIGNED_INT, nullptr);
                        drew = true;
                    }
                } else {
                    glDrawArrays(GL_TRIANGLES, 0, geometry.vertex_count);
                    drew = true;
                }
                if (drew and pass == renderer::Pass::identity)
                    ++identity_draws;
            }

            if (pass == renderer::Pass::identity) {
                if (identity_draws == 0)
                    identity_under = renderer::Integer32{0};
                else
                    identity_under = peek_identity_under(args, viewport.size);
                publish_identity(args, identity_draws, identity_under);
                identity_published = true;
                end_identity_pass(args);
            } else {
                end_pass(pass, args, lighting.shadow);
            }
        }

        if (not identity_published)
            publish_identity(args, 0, renderer::Integer32{0});

        glDepthMask(depth_write_prev);
    }

}
