#include "renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <GL/glew.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <base/logging.h>
#include <base/maybe.h>

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/light.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace rmmr {

    using namespace fqsm::api;
    using namespace api_for_internals;

    Renderer::Renderer()
        : sceneTarget{.fbo = 0, .hdr = 0, .bloomMask = 0, .depth = 0, .size = index2{0, 0}}
        , bloom{.sourceFbo = 0, .scratchFbo = 0, .source = 0, .scratch = 0, .size = index2{0, 0}, .downsampleProgram = 0, .blurProgram = 0, .tonemapProgram = 0}
        , identity{.allFbo = 0, .selectedFbo = 0, .color = 0, .selected = 0, .depth = 0, .size = index2{0, 0}}
        , overlay{.sceneColor = {.fbo = 0, .color = 0, .size = index2{0, 0}}, .overlayColor = {.fbo = 0, .color = 0, .size = index2{0, 0}}, .composeProgram = 0}
        , fullscreen{.vao = 0}
        , passStateBuffer{0}
        , lastStats{.mdiCalls = 0, .indirectDraws = 0}
    {}

    Renderer::~Renderer() {
        if (passStateBuffer)
            glDeleteBuffers(1, &passStateBuffer);
        fullscreen.destroy();
        sceneTarget.destroy();
        bloom.destroy();
        identity.destroy();
        overlay.destroy();
    }

    namespace {

        auto postShaders(Reading world) -> filepath {
            const auto& core = with<system::Core>::get(world, with<system::Core>::singleton(world));
            return core.assets_root / "rmmr" / "shaders";
        }

        auto isSceneColorPass(renderer::Pass pass) -> bool {
            return pass == renderer::Pass::environment or pass == renderer::Pass::opaque or pass == renderer::Pass::transparent or pass == renderer::Pass::sprite or pass == renderer::Pass::gizmo;
        }

        void publishIdentity(Writing world, system::Window::Id window, integer draws, renderer::Integer32 under) {
            auto state = with<system::Window>::modify(world, window);
            state->identityDraws = draws;
            state->current.under = under;
        }

        const struct {
            using Id = material::Semantics::PersistentId;
            Id shadowMap = material::Semantics::id_of("shadowMap");
            Id albedoMap = material::Semantics::id_of("albedoMap");
            Id minerals = material::Semantics::id_of("minerals");
            Id atlasTexture = material::Semantics::id_of("atlasTexture");
            Id atlasEntries = material::Semantics::id_of("atlasEntries");
            Id inverseAtlasSize = material::Semantics::id_of("inverseAtlasSize");
        } semantic{};

        struct ShadowCaster {
            resource::shadow::Runtime::Id runtime;
        };

        struct FrameLighting {
            base::maybe<scene::Light::Id> primary;
            base::maybe<ShadowCaster> shadow;
        };

        auto viewport_aspect_ratio(Reading context, system::Viewport::Id viewport) -> float {
            const auto& quantum = with<system::Viewport>::get(context, viewport);
            const float width = quantum.size.x > 0 ? static_cast<float>(quantum.size.x) : 1.0f;
            const float height = quantum.size.y > 0 ? static_cast<float>(quantum.size.y) : 1.0f;
            return width / height;
        }

        auto frame_lighting(Reading context, scene::Root::Id root, system::Device::Id device) -> FrameLighting {
            FrameLighting lighting{.primary = with<scene::Root>::get(context, root).primaryLight, .shadow = {}};
            if (not lighting.primary)
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
            if (it == material.techniques.end())
                throw std::runtime_error("Renderer: material has no technique for pass");
            return it->second;
        }

        auto directional_to_light(const mat4& lightWorld) -> vec3 {
            return glm::normalize(vec3{mat3{lightWorld}[2]});
        }

        void includePoint(vec3& lo, vec3& hi, bool& any, vec3 point) {
            if (not any) {
                lo = hi = point;
                any = true;
                return;
            }
            lo = glm::min(lo, point);
            hi = glm::max(hi, point);
        }

        auto meshCastsShadow(Reading context, const scene::actor::Mesh::Quantum& mesh) -> bool {
            for (const auto& bucket : mesh.buckets) {
                if (not with<resource::material::Runtime>::exists(context, bucket.material))
                    continue;
                const auto& techniques = with<resource::material::Runtime>::get(context, bucket.material).techniques;
                if (techniques.find(renderer::Pass::shadow) != techniques.end())
                    return true;
            }
            return false;
        }

        auto shadowCubeHalf(Reading context, scene::Root::Id root) -> float {
            constexpr float minHalf = 150.0f;
            constexpr float maxHalf = 1000.0f;
            constexpr float shell = 50.0f;
            float contentHalf = 0.0f;
            for (const auto node : with<scene::Node_group>::get(context, root)) {
                if (not with<scene::actor::Mesh>::exists(context, node))
                    continue;
                if (not meshCastsShadow(context, with<scene::actor::Mesh>::get(context, node)))
                    continue;
                const vec3 a = glm::abs(with<scene::Node>::get(context, node).pose.position);
                contentHalf = glm::max(contentHalf, glm::max(a.x, glm::max(a.y, a.z)));
            }
            return glm::min(maxHalf, glm::max(minHalf, contentHalf + shell));
        }

        auto worldCubeOrtho(vec3 toLight, float half) -> mat4 {
            const vec3 lo{-half, -half, -half};
            const vec3 hi{half, half, half};
            const float radius = half * std::sqrt(3.0f);
            const float pad = 1.0f;
            const vec3 up = std::abs(glm::dot(toLight, vec3{0.0f, 1.0f, 0.0f})) > 0.99f ? vec3{0.0f, 0.0f, 1.0f} : vec3{0.0f, 1.0f, 0.0f};
            const mat4 view = glm::lookAt(toLight * (radius + pad), vec3{0.0f, 0.0f, 0.0f}, up);
            vec3 viewLo{};
            vec3 viewHi{};
            bool any = false;
            for (int corner = 0; corner < 8; ++corner) {
                const vec3 world{(corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y, (corner & 4) ? hi.z : lo.z};
                includePoint(viewLo, viewHi, any, vec3{view * vec4{world, 1.0f}});
            }
            viewLo -= vec3{pad, pad, pad};
            viewHi += vec3{pad, pad, pad};
            const float nearDist = glm::max(0.05f, -viewHi.z);
            const float farDist = glm::max(nearDist + 0.1f, -viewLo.z);
            return glm::ortho(viewLo.x, viewHi.x, viewLo.y, viewHi.y, nearDist, farDist) * view;
        }

        auto light_space_matrix(Reading context, scene::Light::Id light_node, scene::Root::Id root) -> mat4 {
            const auto& light = with<scene::Light>::get(context, light_node);
            if (light.kind == scene::Light::Kind::directional) {
                const vec3 toLight = directional_to_light(scene::Node::Actions::transform(context, light_node));
                return worldCubeOrtho(toLight, shadowCubeHalf(context, root));
            }
            const mat4 light_transform = scene::Node::Actions::transform(context, light_node);
            const glm::vec3 light_position{light_transform[3]};
            const mat4 light_view = glm::lookAt(light_position, glm::vec3{0.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});
            const mat4 light_projection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
            return light_projection * light_view;
        }

        void begin_pass(renderer::Pass pass, Renderer::FrameContext args, base::maybe<ShadowCaster> shadow) {
            if (pass == renderer::Pass::shadow) {
                resource::shadow::Runtime::Actions::bind(args.world, shadow->runtime);
                resource::shadow::Runtime::Actions::clear(args.world, shadow->runtime);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(2.0f, 8.0f);
                return;
            }
            if (pass == renderer::Pass::transparent || pass == renderer::Pass::sprite) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
                return;
            }
            if (pass == renderer::Pass::environment) {
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
                glDisable(GL_POLYGON_OFFSET_FILL);
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

    void Renderer::ensure_material(FrameContext args, renderer::Pass pass, resource::material::Runtime::Id material, resource::shader::Runtime::Id shader, PassDrawState& state, maybe<resource::shadow::Runtime::Id> shadow) {
        const auto material_pass = pass == renderer::Pass::identitySelected ? renderer::Pass::identity : pass;
        const auto& materialQuantum = with<resource::material::Runtime>::get(args.world, material);
        const bool glowSpread = technique_for(materialQuantum, pass).glowSpread;
        if (pass == renderer::Pass::shadow) {
            if (state.bound_shader && *state.bound_shader == shader)
                return;
            with<resource::material::Runtime>::apply(args.world, material, args.window, material_pass);
            bindPassResources(args, material_pass, material, shadow);
            state.bound_shader = shader;
            state.bound_material = material;
            SceneTarget::setGlowWrite(glowSpread);
            return;
        }
        if (state.bound_material && *state.bound_material == material)
            return;
        const bool program_changed = not state.bound_shader || *state.bound_shader != shader;
        if (program_changed) {
            with<resource::material::Runtime>::apply(args.world, material, args.window, material_pass);
            bindPassResources(args, material_pass, material, shadow);
            state.bound_shader = shader;
        }
        state.bound_material = material;
        SceneTarget::setGlowWrite(glowSpread);
    }

    void Renderer::uploadPassState(FrameContext args, base::maybe<scene::Light::Id> primaryLight) {
        const auto& root = with<scene::Root>::get(args.world, args.view.scene);
        const auto aspectRatio = viewport_aspect_ratio(args.world, args.view.viewport);
        auto lightSpace = mat4{1.0f};
        auto lightPositionIntensity = vec4{0.0f};
        auto lightColorRange = vec4{0.0f};
        if (primaryLight) {
            const auto& light = with<scene::Light>::get(args.world, *primaryLight);
            lightSpace = light_space_matrix(args.world, *primaryLight, args.view.scene);
            if (light.kind == scene::Light::Kind::directional) {
                const vec3 toLight = directional_to_light(scene::Node::Actions::transform(args.world, *primaryLight));
                lightPositionIntensity = vec4{toLight, light.intensity};
                lightColorRange = vec4{light.color, 0.0f};
            } else {
                const auto lightTransform = scene::Node::Actions::transform(args.world, *primaryLight);
                lightPositionIntensity = vec4{Pos{lightTransform[3]}, light.intensity};
                lightColorRange = vec4{light.color, light.range};
            }
        }
        const auto state = renderer::PassState{
            .view = scene::Camera::Actions::view(args.world, args.view.camera),
            .projection = scene::Camera::Actions::projection(args.world, args.view.camera, aspectRatio),
            .lightSpace = lightSpace,
            .ambientColorIntensity = vec4{root.ambient, root.ambient_intensity},
            .primaryLightPositionIntensity = lightPositionIntensity,
            .primaryLightColorRange = lightColorRange,
            .shutter = vec4{root.shutter, 0.0f, 0.0f, 0.0f},
        };
        if (not passStateBuffer) {
            glCreateBuffers(1, &passStateBuffer);
            if (not passStateBuffer)
                throw std::runtime_error("Renderer: failed to create pass state buffer");
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
            } else if (binding.id == semantic.minerals) {
                if (not batch.texture3array or not with<resource::texture3array::Runtime>::exists(args.world, *batch.texture3array)) throw std::runtime_error("Renderer: GPU batch missing texture3array");
                const auto& pack = with<resource::texture3array::Runtime>::get(args.world, *batch.texture3array);
                if (pack.layers.size() != 16)
                    throw std::runtime_error("Renderer: minerals pack must have 16 layers");
                const auto unit0 = material::Semantics::binding_of(binding.id);
                for (int layer = 0; layer < 16; ++layer)
                    glBindTextureUnit(unit0 + layer, pack.layers[static_cast<std::size_t>(layer)]);
            } else if (binding.id == semantic.atlasTexture) {
                if (not sprite) throw std::runtime_error("Renderer: atlasTexture requested on non-sprite GPU batch");
                setUniformSampler(binding, with<resource::texture::Runtime>::get(args.world, sprite->texture).handle, material.nearest);
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
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::cohesions, batch.cohesions);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, renderer::StorageBindings::heats, batch.heats);
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

        const auto lighting = frame_lighting(args.world, args.view.scene, args.window);
        base::maybe<resource::shadow::Runtime::Id> shadow{};
        if (lighting.shadow)
            shadow = lighting.shadow->runtime;
        uploadPassState(args, lighting.primary);

        renderer::CommandBuffer commands{};
        scene::Interface::render(args.world, args.view.scene, args.window, commands);

        GLboolean depthWritePrev{};
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWritePrev);

        integer identityDraws = 0;
        renderer::Integer32 identityUnder = renderer::Integer32{0};
        bool identityPublished = false;
        bool identityCleared = false;
        bool sceneBegun = false;
        const auto& viewport = with<system::Viewport>::get(args.world, args.view.viewport);

        for (const auto pass : render_queue_passes) {
            const auto passEmpty = commands.gpu[pass].empty();
            if (pass == renderer::Pass::shadow && not lighting.shadow)
                continue;
            if (pass == renderer::Pass::identitySelected && passEmpty)
                continue;
            if (pass == renderer::Pass::identity && passEmpty) {
                if (not identityCleared) {
                    identity.clear(viewport.size);
                    identityCleared = true;
                }
                publishIdentity(args.world, args.window, 0, renderer::Integer32{0});
                identityPublished = true;
                identity.end(args.world, args.view.viewport);
                continue;
            }
            const bool unlitPass = pass == renderer::Pass::sprite or pass == renderer::Pass::gizmo or pass == renderer::Pass::environment or pass == renderer::Pass::identitySelected or pass == renderer::Pass::identity;
            if (not lighting.primary && not unlitPass) {
                if (not passEmpty)
                    base::message("Renderer: no primary light; skipping draws for pass");
                continue;
            }

            if (isSceneColorPass(pass)) {
                if (not sceneBegun) {
                    sceneTarget.begin(viewport.size, viewport.clear_color);
                    sceneBegun = true;
                } else {
                    sceneTarget.bind(viewport.size);
                }
            }
            if (pass == renderer::Pass::identitySelected) {
                identity.beginSelected(viewport.size);
                identityCleared = true;
            } else if (pass == renderer::Pass::identity) {
                if (not identityCleared) {
                    identity.clear(viewport.size);
                    identityCleared = true;
                }
                identity.beginAll(viewport.size);
            } else {
                begin_pass(pass, args, lighting.shadow);
            }
            PassDrawState passState{};

            auto& gpuBatches = commands.gpu[pass];
            sortGpuByPipeline(pass, gpuBatches);
            for (const auto& batch : gpuBatches) {
                if (batch.drawCount <= renderer::Count{0} or not with<resource::geometry::Runtime>::exists(args.world, batch.geometry))
                    continue;
                apply_blend(pass, batch.renderState.blend);
                if (isSceneColorPass(pass))
                    SceneTarget::setMaskBlendMax();
                else
                    glDisablei(GL_BLEND, 1);
                ensure_material(args, pass, batch.material, batch.shader, passState, shadow);
                drawGpuBatch(args, pass, batch);
                if (pass == renderer::Pass::identity)
                    identityDraws += batch.drawCount;
            }

            if (pass == renderer::Pass::identity) {
                identityUnder = identityDraws == 0 ? renderer::Integer32{0} : identity.peekUnder(args.world, args.window, args.view.viewport, viewport.size);
                publishIdentity(args.world, args.window, identityDraws, identityUnder);
                identityPublished = true;
                identity.end(args.world, args.view.viewport);
            } else if (pass != renderer::Pass::identitySelected) {
                end_pass(pass, args, lighting.shadow);
            }
        }

        if (not identityPublished)
            publishIdentity(args.world, args.window, 0, renderer::Integer32{0});

        if (not sceneBegun)
            sceneTarget.begin(viewport.size, viewport.clear_color);
        const auto& root = with<scene::Root>::get(args.world, args.view.scene);
        const auto shaders = postShaders(args.world);
        bloom.ensurePrograms(shaders);
        bloom.ensure(viewport.size);
        bloom.downsample(sceneTarget.hdr, sceneTarget.bloomMask, fullscreen);
        bloom.blur(root.bloom.radius, fullscreen);
        bloom.tonemapToWindow(args.world, args.view.viewport, sceneTarget.hdr, root.bloom.intensity, fullscreen);

        if (args.overlay) {
            overlay.ensurePrograms(shaders);
            overlay.captureWindow(viewport.size);
            identity.ensure(viewport.size);
            if (not identityCleared) {
                identity.clear(viewport.size);
                identity.end(args.world, args.view.viewport);
            }
            overlay.run(args.world, args.window, args.view.viewport, *args.overlay, args.selection, identity, fullscreen, viewport.size);
            overlay.compose(viewport.size, fullscreen);
            system::Viewport::Actions::activate(args.world, args.view.viewport);
        }

        glDepthMask(depthWritePrev);
    }

    auto Renderer::stats() const -> Stats {
        return lastStats;
    }

}
