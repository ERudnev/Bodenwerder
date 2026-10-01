#pragma once

#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::text {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Pack : Feature<Pack, resource::Unit> {
        struct Document {
            vector<string> lines;
        };
        struct Quantum {
            umap<string, Document> documents;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct LoaderCatalog : Feature<LoaderCatalog, Pack> {
        struct Quantum {
            filename directory;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Schema fragment of doctrine/resources/texts.q1: every aspect declared in this file.
    namespace doctrine {
        auto texts() -> Schema;
    }

}
