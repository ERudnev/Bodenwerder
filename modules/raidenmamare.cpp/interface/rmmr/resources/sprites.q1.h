#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/textures.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::sprite {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    // Rough GPU face — refine later (int VBO layout, bind path, …).
    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            texture::Runtime::Id texture;
            renderer::VertexBuffer vbo;
            integer count;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Catalog entity is Pack — a "sprite" is an entry index, not an aspect.
    // Entry coords are absolute atlas texels: min / max / pivot.
    struct Pack : Feature<Pack, resource::Unit> {
        struct Entry {
            index2 min;
            index2 max;
            index2 pivot;
        };
        struct Quantum {
            texture::Reference texture;
            vector<Entry> entries;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Kenney TextureAtlas XML (x,y,width,height) → absolute min/max/pivot entries.
    struct LoaderKenney : Feature<LoaderKenney, Pack> {
        struct Quantum {
            filename image;
            filename descriptor;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
