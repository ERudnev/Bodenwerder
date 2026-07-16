#pragma once

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Assets : Component<Assets, Manager> {
        struct Quantum {
            maybe<texture::Asset::Id> debug_texture;
        };
        struct Actions : BaseActions {
            static auto add_texture_file(Writing, Id, Unit::Quantum, texture::Asset::Quantum, texture::FromFile::Quantum) -> texture::Asset::Id;
            static auto add_texture_generated(Writing, Id, Unit::Quantum, texture::Asset::Quantum, texture::Generated::Quantum) -> texture::Asset::Id;
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

    struct Runtimes : Component<Runtimes, system::Device> {
        using TextureIdMapping = umap<texture::Asset::Id, texture::Runtime::Id>;

        struct Quantum {
            TextureIdMapping textures_id_mapping;
        };
        struct Actions : BaseActions {
            static void install(Writing, Id);
            static void materialize(Writing, Id, Assets::Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
