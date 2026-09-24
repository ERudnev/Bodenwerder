#include "views/starMap/visuals.h"

#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/builders/materialBuilder.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>

namespace eltanin::views::starmap {

    using namespace fqsm::api;
    using namespace rmmr;
    using rmmr::resource::builders::geometry::CpuPresentation;
    using rmmr::resource::builders::geometry::GeometryGenerator;

    namespace {

        constexpr float mapExtent = 100.0f;
        constexpr float planeLocal = 200.0f;
        constexpr float meshScale = mapExtent / planeLocal;
        constexpr float tensCellLy = 10.0f;
        constexpr float unitCellLy = 1.0f;
        constexpr float axisThicknessAtRef = 0.125f;
        constexpr float dashThicknessAtRef = 0.055f;
        constexpr float dashPeriodAtRef = 2.5f;
        constexpr integer dashMax = 32;
        constexpr float reticleSizeAtRef = 3.2f / 3.0f;
        constexpr float reticleTube = 0.05f;
        constexpr float minStrokePixels = 1.0f;
        constexpr float scaleRefLy = 260.0f;
        constexpr float scaleUnitFull = 25.0f;
        constexpr float scaleTensFull = 80.0f;
        constexpr float gridOpacity = 0.7f;
        constexpr RGB axisXColor{1.0f, 0.12f, 0.12f};
        constexpr RGB axisYColor{0.12f, 1.0f, 0.18f};
        constexpr RGB axisZColor{0.12f, 0.12f, 1.0f};
        constexpr RGB currentPlayerColor{0.18f, 0.95f, 0.32f};
        constexpr RGB viewFocusColor{0.28f, 0.58f, 1.0f};

        auto gauge(float scaleLy) -> float {
            return scaleLy / scaleRefLy;
        }

        auto worldPerPixel(Writing context, scene::Camera::Id camera, system::Window::Id window, float distance) -> float {
            const auto& cam = with<scene::Camera>::get(context, camera);
            const auto fb = with<system::Window>::framebufferSize(context, window);
            const float viewportHeight = static_cast<float>(std::max(fb.y, 1));
            const float aspect = static_cast<float>(fb.x) / viewportHeight;
            const float fovY = 2.0f * std::atan(std::tan(cam.fov_x * 0.5f) / aspect);
            const float depth = std::max(distance, cam.z_near);
            return 2.0f * depth * std::tan(fovY * 0.5f) / viewportHeight;
        }

        auto strokeAtLeastOnePixel(float worldThickness, float pixelWorld) -> float {
            return std::max(worldThickness, pixelWorld * minStrokePixels);
        }

        auto smoothstep(float edge0, float edge1, float value) -> float {
            if (edge1 <= edge0)
                return value < edge0 ? 0.0f : 1.0f;
            const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        auto tensFade(float scaleLy) -> float {
            return smoothstep(scaleUnitFull, scaleTensFull, scaleLy);
        }

        auto unitFade(float scaleLy) -> float {
            return 1.0f - tensFade(scaleLy);
        }

        void appendBox(CpuPresentation& cpu, Pos center, vec3 fullSize, mat3 axes) {
            const auto kube = GeometryGenerator::kube();
            const integer base = static_cast<integer>(cpu.positions.size());
            for (std::size_t vertex = 0; vertex < kube.positions.size(); ++vertex) {
                cpu.positions.push_back(center + axes * (vec3{kube.positions[vertex]} * fullSize));
                cpu.normals.push_back(Pos{glm::normalize(axes * vec3{kube.normals[vertex]})});
                cpu.uv0.push_back(kube.uv0[vertex]);
            }
            for (const auto index : kube.indices)
                cpu.indices.push_back(base + index);
        }

        void appendQuad(CpuPresentation& cpu, Pos a, Pos b, Pos c, Pos d) {
            const vec3 ab = vec3{b} - vec3{a};
            const vec3 ad = vec3{d} - vec3{a};
            const vec3 crossed = glm::cross(ab, ad);
            if (glm::dot(crossed, crossed) < 1e-12f)
                return;
            const Pos normal{glm::normalize(crossed)};
            const integer base = static_cast<integer>(cpu.positions.size());
            cpu.positions.push_back(a);
            cpu.positions.push_back(b);
            cpu.positions.push_back(c);
            cpu.positions.push_back(d);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
            cpu.normals.push_back(normal);
            cpu.uv0.push_back(UV{0.0f, 0.0f});
            cpu.uv0.push_back(UV{1.0f, 0.0f});
            cpu.uv0.push_back(UV{1.0f, 1.0f});
            cpu.uv0.push_back(UV{0.0f, 1.0f});
            cpu.indices.push_back(base + 0);
            cpu.indices.push_back(base + 1);
            cpu.indices.push_back(base + 2);
            cpu.indices.push_back(base + 0);
            cpu.indices.push_back(base + 2);
            cpu.indices.push_back(base + 3);
        }

        auto emptyLit() -> CpuPresentation {
            return CpuPresentation{.layout = primitive::GeometrySemantics::layoutIds(vector<string>{"position", "normal", "uv0"})};
        }

        // Square-tube arcs in XY, facing +Z. Adjacent boxes of the old ring are one strip (no interior caps).
        void appendTubeArc(CpuPresentation& cpu, float radius, float tube, float angle0, float angle1, integer slices) {
            const integer steps = std::max(slices, 1);
            std::array<Pos, 4> previous{};
            for (integer slice = 0; slice <= steps; ++slice) {
                const float t = static_cast<float>(slice) / static_cast<float>(steps);
                const float angle = angle0 + (angle1 - angle0) * t;
                const vec3 radial{std::cos(angle), std::sin(angle), 0.0f};
                const vec3 face{0.0f, 0.0f, 1.0f};
                const Pos center{radial * radius};
                const std::array<Pos, 4> ring{
                    center + Pos{radial * tube + face * tube},
                    center + Pos{-radial * tube + face * tube},
                    center + Pos{-radial * tube - face * tube},
                    center + Pos{radial * tube - face * tube},
                };
                if (slice > 0) {
                    for (integer side = 0; side < 4; ++side) {
                        const integer next = (side + 1) % 4;
                        appendQuad(cpu, previous[side], ring[side], ring[next], previous[next]);
                    }
                } else {
                    appendQuad(cpu, ring[0], ring[3], ring[2], ring[1]);
                }
                if (slice == steps)
                    appendQuad(cpu, ring[0], ring[1], ring[2], ring[3]);
                previous = ring;
            }
        }

        auto reticleMesh() -> CpuPresentation {
            CpuPresentation cpu = emptyLit();
            constexpr float radius = 1.0f;
            constexpr float tube = reticleTube;
            constexpr float slotHalf = 12.0f * std::numbers::pi_v<float> / 180.0f;
            constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
            constexpr float quarter = 0.5f * std::numbers::pi_v<float>;
            constexpr integer slices = 16;
            for (integer cardinal = 0; cardinal < 4; ++cardinal) {
                const float slot = static_cast<float>(cardinal) * quarter;
                appendTubeArc(cpu, radius, tube, slot + slotHalf, slot + quarter - slotHalf, slices);
            }
            constexpr float tickInner = 0.40f;
            constexpr float tickOuter = 0.82f;
            const vec3 face{0.0f, 0.0f, 1.0f};
            for (integer tick = 0; tick < 4; ++tick) {
                const float angle = (0.25f + 0.5f * static_cast<float>(tick)) * std::numbers::pi_v<float>;
                const vec3 radial{std::cos(angle), std::sin(angle), 0.0f};
                const vec3 tangent = glm::normalize(glm::cross(face, radial));
                const float midRadius = 0.5f * (tickInner + tickOuter);
                appendBox(cpu, Pos{radial * midRadius}, vec3{tube * 1.35f, tickOuter - tickInner, tube * 1.35f}, mat3{tangent, radial, face});
            }
            return cpu;
        }

        auto dashMesh(integer dashCount) -> CpuPresentation {
            CpuPresentation cpu = emptyLit();
            const integer count = std::max(dashCount, 1);
            constexpr float dashFraction = 0.58f;
            const float period = 1.0f / static_cast<float>(count);
            for (integer dash = 0; dash < count; ++dash) {
                const float start = -0.5f + static_cast<float>(dash) * period;
                const float length = period * dashFraction;
                appendBox(cpu, Pos{start + 0.5f * length, 0.0f, 0.0f}, vec3{length, 1.0f, 1.0f}, mat3{1.0f});
            }
            return cpu;
        }

        auto installMesh(Writing context, system::Device::Id device, rmmr::resource::Unit::Name name, const CpuPresentation& cpu) -> base::maybe<rmmr::resource::geometry::Asset::Id> {
            const auto manager = with<rmmr::resource::Manager>::singleton(context);
            const auto id = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = std::move(name)});
            with<rmmr::resource::geometry::Asset>::extend(context, id, rmmr::resource::geometry::Asset::Quantum{.entries = {}, .surfaces = {}, .mounts = {}, .entryCatalog = {}, .surfaceCatalogs = {}});
            if (not with<rmmr::resource::geometry::Asset>::install(context, id, device, cpu))
                return {};
            return id;
        }

        auto spawnMesh(Writing context, scene::Root::Id root, rmmr::resource::geometry::Asset::Id geometry, rmmr::resource::material::Asset::Id material, RGB color, Pose pose, vec3 scale) -> base::maybe<scene::actor::Mesh::Id> {
            auto mesh = with<scene::actor::Mesh>::composeOne(context, geometry, material);
            if (not mesh)
                return {};
            return with<scene::Interface>::createMeshActor(context, root, pose, std::move(*mesh), with<scene::actor::MeshState>::defaults(color, 1.0f, scale));
        }

        auto dashCountFor(float length, float period) -> integer {
            if (period <= 1e-6f)
                return 1;
            return std::clamp(static_cast<integer>(std::lround(length / period)), 1, dashMax);
        }

        void poseDrop(Writing context, Marker::Drop& drop, Pos midpoint, HPB rotation, float length, float thickness, float period, const vector<rmmr::resource::geometry::Asset::Id>& dashMeshes, rmmr::resource::material::Asset::Id material) {
            const bool show = length > thickness * 1.5f;
            scene::Node::Actions::setVisible(context, drop.actor, show);
            if (not show)
                return;
            const integer dashes = dashCountFor(length, period);
            if (dashes != drop.dashes and dashes >= 1 and static_cast<std::size_t>(dashes) <= dashMeshes.size()) {
                auto mesh = with<scene::actor::Mesh>::composeOne(context, dashMeshes[static_cast<std::size_t>(dashes - 1)], material);
                if (mesh) {
                    with<scene::actor::Mesh>::replace(context, drop.actor, std::move(*mesh));
                    drop.dashes = dashes;
                }
            }
            with<scene::Node>::modify(context, drop.actor)->pose = Pose::from(midpoint, rotation);
            with<scene::actor::MeshState>::modify(context, drop.actor)->scale = vec3{length, thickness, thickness};
        }

        void poseReticle(Writing context, scene::actor::Mesh::Id actor, Pos point, quat cameraRotation, float size) {
            auto node = with<scene::Node>::modify(context, actor);
            node->pose.position = point;
            node->pose.rotation = cameraRotation;
            with<scene::actor::MeshState>::modify(context, actor)->scale = vec3{size, size, size};
        }

        void poseMarker(Writing context, Marker& marker, Pos point, quat cameraRotation, float reticleSize, float dashThickness, float dashPeriod, const vector<rmmr::resource::geometry::Asset::Id>& dashMeshes, rmmr::resource::material::Asset::Id material) {
            poseReticle(context, marker.reticle, point, cameraRotation, reticleSize);
            poseDrop(context, marker.dropX, Pos{point.x * 0.5f, 0.0f, point.z}, HPB{0.0f, 0.0f, 0.0f}, std::abs(point.x), dashThickness, dashPeriod, dashMeshes, material);
            poseDrop(context, marker.dropY, Pos{point.x, point.y * 0.5f, point.z}, HPB{0.0f, 0.0f, 90.0f}, std::abs(point.y), dashThickness, dashPeriod, dashMeshes, material);
            poseDrop(context, marker.dropZ, Pos{point.x, 0.0f, point.z * 0.5f}, HPB{90.0f, 0.0f, 0.0f}, std::abs(point.z), dashThickness, dashPeriod, dashMeshes, material);
        }

        auto placeMarker(Writing context, scene::Root::Id root, rmmr::resource::geometry::Asset::Id reticle, rmmr::resource::geometry::Asset::Id dash, rmmr::resource::material::Asset::Id material, RGB color) -> base::maybe<Marker> {
            const auto identity = Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f});
            const auto reticleActor = spawnMesh(context, root, reticle, material, color, identity, vec3{1.0f, 1.0f, 1.0f});
            const auto dropX = spawnMesh(context, root, dash, material, color, identity, vec3{1.0f, 1.0f, 1.0f});
            const auto dropY = spawnMesh(context, root, dash, material, color, identity, vec3{1.0f, 1.0f, 1.0f});
            const auto dropZ = spawnMesh(context, root, dash, material, color, identity, vec3{1.0f, 1.0f, 1.0f});
            if (not reticleActor or not dropX or not dropY or not dropZ)
                return {};
            return Marker{.reticle = *reticleActor, .dropX = Marker::Drop{.actor = *dropX, .dashes = dashMax}, .dropY = Marker::Drop{.actor = *dropY, .dashes = dashMax}, .dropZ = Marker::Drop{.actor = *dropZ, .dashes = dashMax}};
        }

        void setGridFade(Writing context, scene::Grid::Id grid, float fade) {
            const float opacity = gridOpacity * fade;
            const bool show = opacity > 0.02f;
            scene::Node::Actions::setVisible(context, grid, show);
            if (not with<scene::Grid>::exists(context, grid) or not with<scene::actor::MeshState>::exists(context, grid))
                return;
            with<scene::Grid>::modify(context, grid)->opacity = opacity;
            with<scene::actor::MeshState>::modify(context, grid)->opacity = opacity;
        }

        void setAxisThickness(Writing context, scene::actor::Mesh::Id actor, vec3 length, float thickness) {
            if (not with<scene::actor::MeshState>::exists(context, actor))
                return;
            with<scene::actor::MeshState>::modify(context, actor)->scale = vec3{
                length.x > 0.0f ? length.x : thickness,
                length.y > 0.0f ? length.y : thickness,
                length.z > 0.0f ? length.z : thickness,
            };
        }

    }

    auto Visuals::addAssets(Writing context) -> bool {
        using Assets = rmmr::resource::Assets;
        using Material = rmmr::resource::material::Asset;
        using Name = rmmr::resource::Unit::Name;
        const auto unlit = with<Assets>::find<Material>(context, Name::from("rmmr", "unlit"));
        if (not unlit) {
            context.refuse("eltanin::views::starmap::Visuals::addAssets: rmmr unlit missing");
            return false;
        }
        chrome = rmmr::resource::builders::material::derive(context, rmmr::resource::builders::material::Derived{
            .name = Name::from("Eltanin", "starMapChrome"),
            .source = *unlit,
            .sourcePass = renderer::Pass::gizmo,
            .targetPass = renderer::Pass::gizmo,
            .program = {},
            .glowSpread = false,
            .renderState = renderer::RenderState{.blend = renderer::BlendMode::additive, .depthTest = renderer::ToggleMode::disabled, .depthWrite = renderer::ToggleMode::disabled, .depthCompare = renderer::DepthCompare::inherit},
        });
        if (not chrome)
            return false;
        return true;
    }

    auto Visuals::place(Writing context, scene::Root::Id root, system::Window::Id window) -> bool {
        renderWindow = window;
        using Assets = rmmr::resource::Assets;
        using Name = rmmr::resource::Unit::Name;
        const auto gridGeometry = with<Assets>::find<rmmr::resource::geometry::Asset>(context, Name::from("Eltanin", "grid"));
        const auto gridMaterial = with<Assets>::find<rmmr::resource::material::Asset>(context, Name::from("rmmr", "grid"));
        const auto kube = with<Assets>::find<rmmr::resource::geometry::Asset>(context, Name::from("Eltanin", "kube"));
        if (not gridGeometry or not gridMaterial or not kube or not chrome) {
            context.refuse("eltanin::views::starmap::Visuals::place: grid assets missing");
            return false;
        }
        const auto tens = with<scene::Interface>::createGrid(context, root, window, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Grid>{.geometry = *gridGeometry, .material = *gridMaterial, .opacity = gridOpacity, .patternScale = meshScale / tensCellLy});
        with<scene::actor::MeshState>::modify(context, tens)->scale = vec3{meshScale, 1.0f, meshScale};
        const auto unit = with<scene::Interface>::createGrid(context, root, window, Pose::from(Pos{0.0f, 0.002f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), item<scene::Grid>{.geometry = *gridGeometry, .material = *gridMaterial, .opacity = 0.0f, .patternScale = meshScale / unitCellLy});
        with<scene::actor::MeshState>::modify(context, unit)->scale = vec3{meshScale, 1.0f, meshScale};
        tensGrid = tens;
        unitGrid = unit;
        auto placeAxis = [&](RGB color, vec3 scale) -> base::maybe<scene::actor::Mesh::Id> {
            auto mesh = with<scene::actor::Mesh>::composeOne(context, *kube, *chrome);
            if (not mesh)
                return {};
            return with<scene::Interface>::createMeshActor(context, root, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}), std::move(*mesh), with<scene::actor::MeshState>::defaults(color, 1.0f, scale));
        };
        const float span = mapExtent * 2.0f;
        axisX = placeAxis(axisXColor, vec3{span, axisThicknessAtRef, axisThicknessAtRef});
        axisZ = placeAxis(axisZColor, vec3{axisThicknessAtRef, axisThicknessAtRef, span});
        axisY = placeAxis(axisYColor, vec3{axisThicknessAtRef, span, axisThicknessAtRef});
        const auto reticle = installMesh(context, window, Name::from("Eltanin", "starMapReticle"), reticleMesh());
        dashMeshes.clear();
        for (integer count = 1; count <= dashMax; ++count) {
            const auto dash = installMesh(context, window, Name::from("Eltanin", std::format("starMapDash{}", count)), dashMesh(count));
            if (not dash) {
                context.refuse("eltanin::views::starmap::Visuals::place: marker meshes failed");
                return false;
            }
            dashMeshes.push_back(*dash);
        }
        if (not reticle or dashMeshes.size() != static_cast<std::size_t>(dashMax)) {
            context.refuse("eltanin::views::starmap::Visuals::place: marker meshes failed");
            return false;
        }
        currentPlayer = placeMarker(context, root, *reticle, dashMeshes.back(), *chrome, currentPlayerColor);
        viewFocus = placeMarker(context, root, *reticle, dashMeshes.back(), *chrome, viewFocusColor);
        if (not currentPlayer or not viewFocus) {
            context.refuse("eltanin::views::starmap::Visuals::place: markers failed");
            return false;
        }
        player = Pos{0.0f, 0.0f, 0.0f};
        focus = Pos{0.0f, 0.0f, 0.0f};
        scaleLy = scaleRefLy;
        scene::Node::Actions::setVisible(context, unit, false);
        return true;
    }

    void Visuals::follow(Writing context, scene::Camera::Id camera) {
        if (not with<scene::Node>::exists(context, camera))
            return;
        const auto& cameraNode = with<scene::Node>::get(context, camera);
        if (with<controller::CameraOrbit>::exists(context, camera)) {
            const auto& orbit = with<controller::CameraOrbit>::get(context, camera);
            focus = orbit.pivot;
            scaleLy = orbit.distance;
        } else {
            focus = Pos{0.0f, 0.0f, 0.0f};
            scaleLy = glm::length(vec3{cameraNode.pose.position});
        }
        const float sized = gauge(scaleLy);
        const float pixelWorld = renderWindow ? worldPerPixel(context, camera, *renderWindow, scaleLy) : 0.0f;
        float reticleSize = reticleSizeAtRef * sized;
        float dashThickness = dashThicknessAtRef * sized;
        const float dashPeriod = dashPeriodAtRef * sized;
        float axisThickness = axisThicknessAtRef * sized;
        if (renderWindow) {
            const float minStroke = pixelWorld * minStrokePixels;
            reticleSize = std::max(reticleSize, minStroke / reticleTube);
            dashThickness = strokeAtLeastOnePixel(dashThickness, pixelWorld);
            axisThickness = strokeAtLeastOnePixel(axisThickness, pixelWorld);
        }
        const float span = mapExtent * 2.0f;
        if (axisX)
            setAxisThickness(context, *axisX, vec3{span, 0.0f, 0.0f}, axisThickness);
        if (axisY)
            setAxisThickness(context, *axisY, vec3{0.0f, span, 0.0f}, axisThickness);
        if (axisZ)
            setAxisThickness(context, *axisZ, vec3{0.0f, 0.0f, span}, axisThickness);
        if (tensGrid)
            setGridFade(context, *tensGrid, tensFade(scaleLy));
        if (unitGrid)
            setGridFade(context, *unitGrid, unitFade(scaleLy));
        if (currentPlayer and chrome)
            poseMarker(context, *currentPlayer, player, cameraNode.pose.rotation, reticleSize, dashThickness, dashPeriod, dashMeshes, *chrome);
        if (viewFocus and chrome)
            poseMarker(context, *viewFocus, focus, cameraNode.pose.rotation, reticleSize, dashThickness, dashPeriod, dashMeshes, *chrome);
    }

}
