#include "renderer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
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
        : scene_color_{.fbo = 0, .color = 0, .size = index2{0, 0}}
        , overlay_color_{.fbo = 0, .color = 0, .size = index2{0, 0}}
        , identity_{.all_fbo = 0, .selected_fbo = 0, .color = 0, .selected = 0, .depth = 0, .size = index2{0, 0}}
        , fullscreen_vao_{0}
        , passStateBuffer{0}
        , lastStats{.mdiCalls = 0, .indirectDraws = 0}
    {}

    Renderer::~Renderer() {
        if (passStateBuffer)
            glDeleteBuffers(1, &passStateBuffer);
        if (fullscreen_vao_)
            glDeleteVertexArrays(1, &fullscreen_vao_);
        auto release = [](ColorTarget& target) {
            if (target.fbo)
                glDeleteFramebuffers(1, &target.fbo);
            if (target.color)
                glDeleteTextures(1, &target.color);
        };
        release(scene_color_);
        release(overlay_color_);
        if (identity_.all_fbo)
            glDeleteFramebuffers(1, &identity_.all_fbo);
        if (identity_.selected_fbo)
            glDeleteFramebuffers(1, &identity_.selected_fbo);
        if (identity_.color)
            glDeleteTextures(1, &identity_.color);
        if (identity_.selected)
            glDeleteTextures(1, &identity_.selected);
        if (identity_.depth)
            glDeleteTextures(1, &identity_.depth);
    }

    void Renderer::ensure_color_target(ColorTarget& target, index2 size, const char* label) {
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        if (target.fbo and target.size.x == size.x and target.size.y == size.y)
            return;

        if (target.fbo) {
            glDeleteFramebuffers(1, &target.fbo);
            target.fbo = 0;
        }
        if (target.color) {
            glDeleteTextures(1, &target.color);
            target.color = 0;
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &target.color);
        glTextureStorage2D(target.color, 1, GL_RGBA8, width, height);
        glTextureParameteri(target.color, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(target.color, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(target.color, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(target.color, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glCreateFramebuffers(1, &target.fbo);
        glNamedFramebufferTexture(target.fbo, GL_COLOR_ATTACHMENT0, target.color, 0);
        const GLenum draw_buffers[]{GL_COLOR_ATTACHMENT0};
        glNamedFramebufferDrawBuffers(target.fbo, 1, draw_buffers);
        if (glCheckNamedFramebufferStatus(target.fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error(std::string("Renderer: ") + label + " framebuffer incomplete");
        }
        target.size = size;
    }

    void Renderer::ensure_identity_target(index2 size) {
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        if (identity_.all_fbo and identity_.selected_fbo and identity_.size.x == size.x and identity_.size.y == size.y)
            return;

        if (identity_.all_fbo) {
            glDeleteFramebuffers(1, &identity_.all_fbo);
            identity_.all_fbo = 0;
        }
        if (identity_.selected_fbo) {
            glDeleteFramebuffers(1, &identity_.selected_fbo);
            identity_.selected_fbo = 0;
        }
        if (identity_.color) {
            glDeleteTextures(1, &identity_.color);
            identity_.color = 0;
        }
        if (identity_.selected) {
            glDeleteTextures(1, &identity_.selected);
            identity_.selected = 0;
        }
        if (identity_.depth) {
            glDeleteTextures(1, &identity_.depth);
            identity_.depth = 0;
        }

        auto make_id_texture = [&](renderer::Texture& texture) {
            glCreateTextures(GL_TEXTURE_2D, 1, &texture);
            glTextureStorage2D(texture, 1, GL_R32UI, width, height);
            glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };
        make_id_texture(identity_.color);
        make_id_texture(identity_.selected);

        glCreateTextures(GL_TEXTURE_2D, 1, &identity_.depth);
        glTextureStorage2D(identity_.depth, 1, GL_DEPTH_COMPONENT24, width, height);
        glTextureParameteri(identity_.depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(identity_.depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        auto make_fbo = [&](renderer::Framebuffer& fbo, renderer::Texture color, const char* label) {
            glCreateFramebuffers(1, &fbo);
            glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, color, 0);
            glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, identity_.depth, 0);
            const GLenum draw_buffers[]{GL_COLOR_ATTACHMENT0};
            glNamedFramebufferDrawBuffers(fbo, 1, draw_buffers);
            if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                throw std::runtime_error(std::string("Renderer: ") + label + " framebuffer incomplete");
            }
        };
        make_fbo(identity_.all_fbo, identity_.color, "identity");
        make_fbo(identity_.selected_fbo, identity_.selected, "identitySelected");

        identity_.size = size;
    }

    void Renderer::clear_identity_feature(index2 size) {
        ensure_identity_target(size);
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        glViewport(0, 0, width, height);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL); // selected then all: same-Z must still write all-ID
        glDepthMask(GL_TRUE);
        const GLuint clear_alias[]{0u};
        glBindFramebuffer(GL_FRAMEBUFFER, identity_.selected_fbo);
        glClearBufferuiv(GL_COLOR, 0, clear_alias);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, identity_.all_fbo);
        glClearBufferuiv(GL_COLOR, 0, clear_alias);
        // depth already cleared via shared attachment
    }

    void Renderer::begin_identity_selected_pass(index2 size) {
        clear_identity_feature(size);
        glBindFramebuffer(GL_FRAMEBUFFER, identity_.selected_fbo);
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        glViewport(0, 0, width, height);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
    }

    void Renderer::begin_identity_pass(index2 size) {
        ensure_identity_target(size);
        glBindFramebuffer(GL_FRAMEBUFFER, identity_.all_fbo);
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        glViewport(0, 0, width, height);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
    }

    void Renderer::end_identity_pass(FrameContext args) {
        glDepthFunc(GL_LESS);
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

    void Renderer::capture_scene_color(index2 size) {
        ensure_color_target(scene_color_, size, "sceneColor");
        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scene_color_.fbo);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Renderer::run_overlay(FrameContext args, index2 size) {
        if (not args.overlay)
            return;
        const auto& runtimes = with<resource::Runtimes>::get(args.world, args.window);
        const auto overlay_it = runtimes.overlays_id_mapping.find(*args.overlay);
        if (overlay_it == runtimes.overlays_id_mapping.end())
            return;

        const auto& overlay = with<resource::overlay::Runtime>::get(args.world, overlay_it->second);
        const auto& shader = with<resource::shader::Runtime>::get(args.world, overlay.shader);
        const int divisor = resource::overlay::scale_divisor(overlay.scale);
        const index2 effect_size{
            std::max(static_cast<int>(size.x) / divisor, 1),
            std::max(static_cast<int>(size.y) / divisor, 1),
        };

        ensure_identity_target(size);
        ensure_color_target(overlay_color_, effect_size, "overlay");
        const int width = std::max(static_cast<int>(effect_size.x), 1);
        const int height = std::max(static_cast<int>(effect_size.y), 1);

        glBindFramebuffer(GL_FRAMEBUFFER, overlay_color_.fbo);
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shader.handle);
        if (fullscreen_vao_ == 0)
            glCreateVertexArrays(1, &fullscreen_vao_);
        const auto id_scene = material::Semantics::id_of("sceneColor");
        const auto id_ident = material::Semantics::id_of("identiffyMap");
        const auto id_selected_map = material::Semantics::id_of("selectedMap");
        const auto id_texel = material::Semantics::id_of("texelSize");
        const auto id_under = material::Semantics::id_of("under");
        const auto id_selected_count = material::Semantics::id_of("selectedCount");
        const auto id_selected = material::Semantics::id_of("selected");
        const auto under = with<system::Window>::get(args.world, args.window).current.under;
        const int selected_count = std::min(static_cast<int>(args.selection.size()), resource::overlay::selection_capacity);
        for (const auto& binding : overlay.bindings) {
            if (binding.id == id_scene) {
                glBindTextureUnit(material::Semantics::binding_of(binding.id), scene_color_.color);
            } else if (binding.id == id_ident) {
                glBindTextureUnit(material::Semantics::binding_of(binding.id), identity_.color);
            } else if (binding.id == id_selected_map) {
                glBindTextureUnit(material::Semantics::binding_of(binding.id), identity_.selected);
            } else if (binding.location < 0) {
                continue;
            } else if (binding.id == id_texel) {
                glUniform2f(binding.location, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
            } else if (binding.id == id_under) {
                glUniform1ui(binding.location, under);
            } else if (binding.id == id_selected_count) {
                glUniform1i(binding.location, selected_count);
            } else if (binding.id == id_selected) {
                if (selected_count > 0)
                    glUniform1uiv(binding.location, selected_count, args.selection.data());
            }
        }

        glBindVertexArray(fullscreen_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        system::Viewport::Actions::activate(args.world, args.view.viewport);
    }

    void Renderer::compose_overlay(index2 size) {
        // Engine paste: alpha-blend overlay RGBA onto the default framebuffer.
        static GLuint compose_program = 0;
        if (compose_program == 0) {
            const char* vs = R"(#version 450 core
out vec2 vUv;
void main() {
    const vec2 pos[3] = vec2[](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    vec2 p = pos[gl_VertexID];
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
})";
            const char* fs = R"(#version 450 core
layout(binding = 0) uniform sampler2D u_overlay;
in vec2 vUv;
out vec4 fragColor;
void main() { fragColor = texture(u_overlay, vUv); })";
            const auto compile = [](GLenum type, const char* src) {
                GLuint shader = glCreateShader(type);
                glShaderSource(shader, 1, &src, nullptr);
                glCompileShader(shader);
                GLint ok = 0;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
                if (not ok)
                    throw std::runtime_error("Renderer: compose shader compile failed");
                return shader;
            };
            const GLuint v = compile(GL_VERTEX_SHADER, vs);
            const GLuint f = compile(GL_FRAGMENT_SHADER, fs);
            compose_program = glCreateProgram();
            glAttachShader(compose_program, v);
            glAttachShader(compose_program, f);
            glLinkProgram(compose_program);
            glDeleteShader(v);
            glDeleteShader(f);
            GLint linked = 0;
            glGetProgramiv(compose_program, GL_LINK_STATUS, &linked);
            if (not linked)
                throw std::runtime_error("Renderer: compose program link failed");
        }

        const int width = std::max(static_cast<int>(size.x), 1);
        const int height = std::max(static_cast<int>(size.y), 1);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(compose_program);
        glBindTextureUnit(0, overlay_color_.color);
        if (fullscreen_vao_ == 0)
            glCreateVertexArrays(1, &fullscreen_vao_);
        glBindVertexArray(fullscreen_vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glUseProgram(0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    namespace {

        // Authoring names resolved once; draw/bind path compares PersistentId only.
        const struct {
            using Id = material::Semantics::PersistentId;
            Id shadowMap = material::Semantics::id_of("shadowMap");
            Id albedoMap = material::Semantics::id_of("albedoMap");
            Id atlasTexture = material::Semantics::id_of("atlasTexture");
            Id atlasEntries = material::Semantics::id_of("atlasEntries");
            Id inverseAtlasSize = material::Semantics::id_of("inverseAtlasSize");
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

        void setUniform(const resource::Uniform::Binding& binding, const vec2& value) {
            glUniform2f(binding.location, value.x, value.y);
        }

        void setUniformSampler(const resource::Uniform::Binding& binding, GLuint texture, bool nearest = false) {
            const auto unit = material::Semantics::binding_of(binding.id);
            if (nearest) {
                glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }
            glBindTextureUnit(unit, texture);
        }

        void setUniformSsbo(const resource::Uniform::Binding& binding, GLuint buffer) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, material::Semantics::binding_of(binding.id), buffer);
        }

        auto bindingActive(const resource::Uniform::Binding& binding) -> bool {
            return material::Semantics::isBoundResource(binding.type) or binding.location >= 0;
        }

        auto technique_for(const resource::material::Runtime::Quantum& material, renderer::Pass pass) -> const resource::material::Runtime::Technique& {
            const auto technique_pass = pass == renderer::Pass::identitySelected ? renderer::Pass::identity : pass;
            const auto it = material.techniques.find(technique_pass);
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
                glDepthFunc(GL_LESS);
            }
        }

        // shadow is offscreen; identity is offscreen pick; environment is first color on the main FB…
        constexpr std::array<renderer::Pass, 8> render_queue_passes{
            renderer::Pass::shadow,
            renderer::Pass::environment,
            renderer::Pass::opaque,
            renderer::Pass::transparent,
            renderer::Pass::sprite,
            renderer::Pass::gizmo,
            renderer::Pass::identitySelected,
            renderer::Pass::identity,
        };

        void apply_blend(renderer::Pass pass, renderer::BlendMode blend) {
            // identity / identitySelected share depth: selected writes first, all-ID must LEQUAL same-Z
            // (begin_identity_* sets LEQUAL; do not stomp it back to LESS for inherit/opaque).
            const bool identityPass = pass == renderer::Pass::identitySelected or pass == renderer::Pass::identity;
            if (blend == renderer::BlendMode::inherit) {
                if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite || pass == renderer::Pass::gizmo) {
                    blend = renderer::BlendMode::alpha;
                } else {
                    if (not identityPass)
                        glDepthFunc(GL_LESS);
                    return;
                }
            }

            if (blend == renderer::BlendMode::additive) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                // Additive overlays (e.g. clipboard ghosts): pass same-Z so highlight sits on existing opaque.
                glDepthFunc(GL_LEQUAL);
                return;
            }

            if (not identityPass)
                glDepthFunc(GL_LESS);

            if (blend == renderer::BlendMode::alpha) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                return;
            }

            if (blend == renderer::BlendMode::premultiplied) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            }
        }

        void sortGpuByPipeline(renderer::Pass pass, renderer::SeparateBuffers<renderer::GpuBatch>::Buffer& batches) {
            std::sort(batches.begin(), batches.end(), [pass](const renderer::GpuBatch& left, const renderer::GpuBatch& right) {
                if (left.shader != right.shader) return left.shader < right.shader;
                if (pass != renderer::Pass::shadow and left.material != right.material) return left.material < right.material;
                return left.geometry < right.geometry;
            });
        }

    } // namespace

    void Renderer::ensure_material(
        FrameContext args,
        renderer::Pass pass,
        resource::material::Runtime::Id material,
        resource::shader::Runtime::Id shader,
        PassDrawState& state,
        base::maybe<resource::shadow::Runtime::Id> shadow)
    {
        const auto material_pass = pass == renderer::Pass::identitySelected ? renderer::Pass::identity : pass;
        // Depth-only shadow technique has no material-unique samplers: cache by program.
        if (pass == renderer::Pass::shadow) {
            if (state.bound_shader && *state.bound_shader == shader) {
                return;
            }
            with<resource::material::Runtime>::apply(args.world, material, args.window, material_pass);
            bindPassResources(args, material_pass, material, shadow);
            state.bound_shader = shader;
            state.bound_material = material;
            return;
        }

        if (state.bound_material && *state.bound_material == material) {
            return;
        }

        const bool program_changed = not state.bound_shader || *state.bound_shader != shader;

        if (program_changed) {
            with<resource::material::Runtime>::apply(args.world, material, args.window, material_pass);
            bindPassResources(args, material_pass, material, shadow);
            state.bound_shader = shader;
        }

        state.bound_material = material;
    }

    void Renderer::uploadPassState(FrameContext args, base::maybe<scene::Light::Id> primaryLight) {
        const auto& root = with<scene::Root>::get(args.world, args.view.scene);
        const auto aspectRatio = viewport_aspect_ratio(args.world, args.view.viewport);
        auto lightSpace = mat4{1.0f};
        auto lightPositionIntensity = vec4{0.0f};
        auto lightColorRange = vec4{0.0f};
        if (primaryLight) {
            const auto& light = with<scene::Light>::get(args.world, *primaryLight);
            lightSpace = light_space_matrix(args.world, *primaryLight);
            const auto lightTransform = scene::Node::Actions::transform(args.world, *primaryLight);
            lightPositionIntensity = vec4{Pos{lightTransform[3]}, light.intensity};
            lightColorRange = vec4{light.color, light.range};
        }
        const auto state = renderer::PassState{
            .view = scene::Camera::Actions::view(args.world, args.view.camera),
            .projection = scene::Camera::Actions::projection(args.world, args.view.camera, aspectRatio),
            .lightSpace = lightSpace,
            .ambientColorIntensity = vec4{root.ambient, root.ambient_intensity},
            .primaryLightPositionIntensity = lightPositionIntensity,
            .primaryLightColorRange = lightColorRange,
        };
        if (not passStateBuffer) {
            glCreateBuffers(1, &passStateBuffer);
            if (not passStateBuffer) throw std::runtime_error("Renderer: failed to create pass state buffer");
            glNamedBufferStorage(passStateBuffer, sizeof(renderer::PassState), &state, GL_DYNAMIC_STORAGE_BIT);
        } else {
            glNamedBufferSubData(passStateBuffer, 0, sizeof(renderer::PassState), &state);
        }
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, passStateBuffer);
    }

    void Renderer::bindPassResources(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, base::maybe<resource::shadow::Runtime::Id> shadow) {
        const auto& materialQuantum = with<resource::material::Runtime>::get(args.world, material);
        const auto& technique = technique_for(materialQuantum, pass);
        for (const auto& binding : technique.bindings) {
            if (binding.id != semantic.shadowMap) continue;
            if (not shadow) throw std::runtime_error("Renderer: material expects shadowMap but no shadow-casting light");
            setUniformSampler(binding, with<resource::shadow::Runtime>::get(args.world, *shadow).depth);
        }
    }

    void Renderer::drawGpuBatch(FrameContext args, renderer::Pass pass, const renderer::GpuBatch& batch) {
        const auto& material = with<resource::material::Runtime>::get(args.world, batch.material);
        const auto& technique = technique_for(material, pass);
        const resource::sprite::Runtime::Quantum* sprite = nullptr;
        if (batch.sprite) {
            if (not with<resource::sprite::Runtime>::exists(args.world, *batch.sprite)) throw std::runtime_error("Renderer: GPU batch sprite runtime missing");
            sprite = &with<resource::sprite::Runtime>::get(args.world, *batch.sprite);
        }
        for (const auto& binding : technique.bindings) {
            if (not bindingActive(binding)) continue;
            if (binding.id == semantic.albedoMap) {
                if (not batch.texpack or not with<resource::texpack::Runtime>::exists(args.world, *batch.texpack)) throw std::runtime_error("Renderer: GPU batch missing texpack");
                setUniformSampler(binding, with<resource::texpack::Runtime>::get(args.world, *batch.texpack).handle, material.nearest);
            } else if (binding.id == semantic.atlasTexture) {
                if (not sprite) throw std::runtime_error("Renderer: atlasTexture requested on non-sprite GPU batch");
                const auto& texture = with<resource::texture::Runtime>::get(args.world, sprite->texture);
                setUniformSampler(binding, texture.handle, material.nearest);
            } else if (binding.id == semantic.atlasEntries) {
                if (not sprite) throw std::runtime_error("Renderer: atlasEntries requested on non-sprite GPU batch");
                setUniformSsbo(binding, sprite->entries_buffer);
            } else if (binding.id == semantic.inverseAtlasSize) {
                if (not sprite) throw std::runtime_error("Renderer: inverseAtlasSize requested on non-sprite GPU batch");
                const auto& texture = with<resource::texture::Runtime>::get(args.world, sprite->texture);
                const float inverseWidth = texture.size.x > 0 ? 1.0f / static_cast<float>(texture.size.x) : 0.0f;
                const float inverseHeight = texture.size.y > 0 ? 1.0f / static_cast<float>(texture.size.y) : 0.0f;
                setUniform(binding, vec2{inverseWidth, inverseHeight});
            }
        }

        const auto& geometry = with<resource::geometry::Runtime>::get(args.world, batch.geometry);
        glBindVertexArray(geometry.vao);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::actorState, batch.actorState);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::poses, batch.poses);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::drawMetadata, batch.drawMetadata, batch.metadataByteOffset, batch.metadataByteSize);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::surfacePalette, batch.surfacePalette);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::primitiveSurfaces, geometry.primitiveSurfaces);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, batch.indirect);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr, batch.drawCount, sizeof(renderer::DrawElementsIndirect));
        ++lastStats.mdiCalls;
        lastStats.indirectDraws += batch.drawCount;
    }

    void Renderer::render(FrameContext args) {
        lastStats = Stats{.mdiCalls = 0, .indirectDraws = 0};
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
        base::maybe<scene::Light::Id> primaryLight{};
        if (not lighting.lights.empty()) primaryLight = lighting.lights.front();
        uploadPassState(args, primaryLight);

        renderer::CommandBuffer commands{};
        scene::Interface::render(args.world, args.view.scene, args.window, commands);

        GLboolean depth_write_prev{};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write_prev);

        integer identity_draws = 0;
        renderer::Integer32 identity_under = renderer::Integer32{0};
        bool identity_published = false;
        bool identity_feature_cleared = false;

        for (const auto pass : render_queue_passes) {
            const auto passEmpty = commands.gpu[pass].empty();
            if (pass == renderer::Pass::shadow && not lighting.shadow) {
                continue;
            }
            if (pass == renderer::Pass::identitySelected && passEmpty) {
                continue;
            }
            if (pass == renderer::Pass::identity && passEmpty) {
                if (not identity_feature_cleared) {
                    clear_identity_feature(with<system::Viewport>::get(args.world, args.view.viewport).size);
                    identity_feature_cleared = true;
                }
                identity_under = renderer::Integer32{0};
                publish_identity(args, 0, identity_under);
                identity_published = true;
                end_identity_pass(args);
                continue;
            }
            const bool unlit_pass = pass == renderer::Pass::sprite
                || pass == renderer::Pass::gizmo
                || pass == renderer::Pass::environment
                || pass == renderer::Pass::identitySelected
                || pass == renderer::Pass::identity;
            if (lighting.lights.empty() && not unlit_pass) {
                if (not passEmpty) base::message("Renderer: no light; skipping draws for pass");
                continue;
            }

            const auto& viewport = with<system::Viewport>::get(args.world, args.view.viewport);
            if (pass == renderer::Pass::identitySelected) {
                begin_identity_selected_pass(viewport.size);
                identity_feature_cleared = true;
            } else if (pass == renderer::Pass::identity) {
                if (not identity_feature_cleared) {
                    clear_identity_feature(viewport.size);
                    identity_feature_cleared = true;
                }
                begin_identity_pass(viewport.size);
            } else {
                begin_pass(pass, args, lighting.shadow);
            }
            PassDrawState pass_state{};

            auto& gpuBatches = commands.gpu[pass];
            sortGpuByPipeline(pass, gpuBatches);

            for (const auto& batch : gpuBatches) {
                if (batch.drawCount <= renderer::Count{0} or not with<resource::geometry::Runtime>::exists(args.world, batch.geometry)) continue;
                apply_blend(pass, batch.renderState.blend);
                ensure_material(args, pass, batch.material, batch.shader, pass_state, shadow);
                drawGpuBatch(args, pass, batch);
                if (pass == renderer::Pass::identity) identity_draws += batch.drawCount;
            }

            if (pass == renderer::Pass::identity) {
                if (identity_draws == 0)
                    identity_under = renderer::Integer32{0};
                else
                    identity_under = peek_identity_under(args, viewport.size);
                publish_identity(args, identity_draws, identity_under);
                identity_published = true;
                end_identity_pass(args);
            } else if (pass == renderer::Pass::identitySelected) {
                // Shared depth kept; all-ID pass follows without restoring the main FB.
            } else {
                end_pass(pass, args, lighting.shadow);
            }
        }

        if (not identity_published)
            publish_identity(args, 0, renderer::Integer32{0});

        if (args.overlay) {
            const auto& viewport = with<system::Viewport>::get(args.world, args.view.viewport);
            capture_scene_color(viewport.size);
            ensure_identity_target(viewport.size);
            if (not identity_feature_cleared) {
                clear_identity_feature(viewport.size);
                end_identity_pass(args);
            }
            run_overlay(args, viewport.size);
            compose_overlay(viewport.size);
            system::Viewport::Actions::activate(args.world, args.view.viewport);
        }

        glDepthMask(depth_write_prev);
    }

    auto Renderer::stats() const -> Stats {
        return lastStats;
    }

}
