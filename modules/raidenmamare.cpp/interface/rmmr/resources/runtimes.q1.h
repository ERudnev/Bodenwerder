#pragma once

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Assets : Component<Assets, Manager> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto add_texture_loader(Writing, Id, Unit::Quantum, texture::Asset::Quantum, texture::Loader::Quantum) -> texture::Asset::Id;
            static auto add_texture_generator(Writing, Id, Unit::Quantum, texture::Asset::Quantum, texture::Generator::Quantum) -> texture::Asset::Id;
            static auto add_shader_loader(Writing, Id, Unit::Quantum, shader::Asset::Quantum, shader::Loader::Quantum) -> shader::Asset::Id;
            static auto add_material(Writing, Id, Unit::Quantum, material::Asset::Quantum, material::Composer::Quantum) -> material::Asset::Id;
            static auto add_shadow_allocator(Writing, Id, Unit::Quantum, shadow::Asset::Quantum, shadow::Allocator::Quantum) -> shadow::Asset::Id;
            static auto add_geometry_loader(Writing, Id, Unit::Quantum, geometry::Asset::Quantum, geometry::Loader::Quantum) -> geometry::Asset::Id;
            static auto add_geometry_generator(Writing, Id, Unit::Quantum, geometry::Asset::Quantum, geometry::Generator::Quantum) -> geometry::Asset::Id;
            static void extend(Writing, Manager::Id, filepath path);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct DeviceRuntimes : Component<DeviceRuntimes, system::Device> {
        struct Quantum {
            Assets::Id assets;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime_group : Group<Runtime_group, DeviceRuntimes, texture::Runtime> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct ShaderRuntime_group : Group<ShaderRuntime_group, DeviceRuntimes, shader::Runtime> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct MaterialRuntime_group : Group<MaterialRuntime_group, DeviceRuntimes, material::Runtime> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct ShadowRuntime_group : Group<ShadowRuntime_group, DeviceRuntimes, shadow::Runtime> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct GeometryRuntime_group : Group<GeometryRuntime_group, DeviceRuntimes, geometry::Runtime> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtimes : Component<Runtimes, system::Device> {
        struct Quantum {
            umap<texture::Asset::Id, texture::Runtime::Id> textures_id_mapping;
            umap<shader::Asset::Id, shader::Runtime::Id> shaders_id_mapping;
            umap<material::Asset::Id, material::Runtime::Id> materials_id_mapping;
            umap<shadow::Asset::Id, shadow::Runtime::Id> shadows_id_mapping;
            umap<geometry::Asset::Id, geometry::Runtime::Id> geometries_id_mapping;
        };
        struct Actions : BaseActions {
            static void install(Writing, Id);
            static void materialize(Writing, Id, Assets::Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
