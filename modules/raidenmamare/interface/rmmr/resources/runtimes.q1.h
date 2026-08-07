#pragma once

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>
#include <fQSM/meta/categories.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Assets : Component<Assets, Manager> {
        struct Quantum {};
        struct Global {
            optional<Id> singleton{};
        };
        struct Actions : BaseActions {
            static auto singleton(Reading) -> optional<Id>;
            static auto add_texture_loader(Writing, Unit::Name, texture::Loader::Quantum) -> texture::Asset::Id;
            static auto add_texture_generator(Writing, Unit::Name, texture::Generator::Quantum) -> texture::Asset::Id;
            static auto add_shader_loader(Writing, Unit::Name, shader::Loader::Quantum) -> shader::Asset::Id;
            static auto add_material(Writing, Unit::Name, material::Asset::Quantum) -> material::Asset::Id;
            static auto add_overlay(Writing, Unit::Name, overlay::Asset::Quantum) -> overlay::Asset::Id;
            static auto add_shadow_allocator(Writing, Unit::Name, shadow::Allocator::Quantum) -> shadow::Asset::Id;
            static auto add_geometry_loader(Writing, Unit::Name, geometry::Loader::Quantum) -> geometry::Asset::Id;
            static auto add_geometry_generator(Writing, Unit::Name, geometry::Generator::Quantum) -> geometry::Asset::Id;
            static auto add_sprites_kenney(Writing, Unit::Name, sprite::LoaderKenney::Quantum) -> sprite::Pack::Id;
            static auto add_meshpack_objs_loader(Writing, Unit::Name, meshpack::LoaderObjs::Quantum) -> meshpack::Asset::Id;
            static auto add_meshpack_lwo_loader(Writing, Unit::Name, meshpack::LoaderLwo::Quantum) -> meshpack::Asset::Id;
            static auto compose_material(Writing, Unit::Name, filename, material::Asset::Id base) -> material::Asset::Id;
            // Q1: ?find<R as feature of Unit>(Unit::Name)-> #R?
            template<::fqsm::meta::category::Any Meta>
            static auto find(Reading, Unit::Name) -> optional<typename Meta::Id>;
            static void extend(Writing, filepath path);
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

    struct OverlayRuntime_group : Group<OverlayRuntime_group, DeviceRuntimes, overlay::Runtime> {
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

    struct SpriteRuntime_group : Group<SpriteRuntime_group, DeviceRuntimes, sprite::Runtime> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtimes : Component<Runtimes, system::Device> {
        struct Quantum {
            umap<texture::Asset::Id, texture::Runtime::Id> textures_id_mapping;
            umap<shader::Asset::Id, shader::Runtime::Id> shaders_id_mapping;
            umap<material::Asset::Id, material::Runtime::Id> materials_id_mapping;
            umap<overlay::Asset::Id, overlay::Runtime::Id> overlays_id_mapping;
            umap<shadow::Asset::Id, shadow::Runtime::Id> shadows_id_mapping;
            umap<geometry::Asset::Id, geometry::Runtime::Id> geometries_id_mapping;
            umap<sprite::Pack::Id, sprite::Runtime::Id> sprites_id_mapping;
        };
        struct Actions : BaseActions {
            static void install(Writing, Id);
            static void materialize(Writing, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    template<::fqsm::meta::category::Any Meta>
    auto Assets::Actions::find(Reading context, Unit::Name name) -> optional<typename Meta::Id> {
        for (const auto [id, unit] : context->aspect<Unit>().items()) {
            if (unit.name != name)
                continue;
            if (not with<Meta>::exists(context, id))
                continue;
            return id;
        }
        return {};
    }

}
