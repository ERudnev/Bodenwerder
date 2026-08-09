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
            geometry::EntryId entry;
            umap<geometry::SurfaceId, material::Instance> surfaces;
        };
        struct Resolved {
            geometry::Asset::Id geometry;
            geometry::EntryId entry;
            umap<geometry::SurfaceId, material::Instance> surfaces;
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

    // Name bindings are transient loader state until geometry catalogs are materialized.
    struct LoaderObjs : Feature<LoaderObjs, Asset> {
        struct PendingEntry {
            geometry::Asset::Id geometry;
            umap<string, material::Instance> surfaces;
        };
        struct Quantum {
            filename file;
            umap<string, PendingEntry> pending;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
            static void finalize(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct LoaderLwo : Feature<LoaderLwo, Asset> {
        struct Quantum {
            filename file;
            base::maybe<geometry::Asset::Id> geometry;
            umap<string, material::Instance> pending;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
            static void finalize(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
