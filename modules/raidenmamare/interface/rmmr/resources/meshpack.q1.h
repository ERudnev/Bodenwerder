#pragma once

#include <base/maybe.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::meshpack {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Entry {
            geometry::Asset::Id geometry;
            umap<string, material::Instance> parts;
        };
        struct Resolved {
            geometry::Asset::Id geometry;
            umap<string, material::Instance> parts;
            base::maybe<texpack::Pack::Id> texpack;
        };
        struct Quantum {
            base::maybe<texpack::Pack::Id> texpack;
            umap<string, Entry> entries;
        };
        struct Actions : BaseActions {
            static auto resolve(Reading, Id, string name) -> optional<Resolved>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct LoaderObjs : Feature<LoaderObjs, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct LoaderLwo : Feature<LoaderLwo, Asset> {
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
