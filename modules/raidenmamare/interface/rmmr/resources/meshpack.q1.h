#pragma once

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::meshpack {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    // Catalog of ready looks. No Runtime — geometry/materials bake on their own.
    struct Asset : Feature<Asset, resource::Unit> {
        struct Entry {
            struct Binding {
                string alias;
                string materialInstancePlaceholder;
            };
            geometry::Asset::Id geometry;
            umap<string, Binding> materials;
        };
        struct Resolved {
            geometry::Asset::Id geometry;
            umap<string, material::Asset::Id> materials;
        };
        struct Quantum {
            umap<string, material::Asset::Id> materials;
            umap<string, Entry> entries;
        };
        struct Actions : BaseActions {
            static auto resolve(Reading, Id, string name) -> optional<Resolved>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
