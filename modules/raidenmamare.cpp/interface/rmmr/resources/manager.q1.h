#pragma once

#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Manager : Component<Manager, system::Core> {
        struct Quantum {
            filepath location;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Unit : Entity<Unit> {
        using Name = string;

        struct Reference {
            Id id;
            Name backup;
        };

        struct Quantum {
            Manager::Id manager;
            Name name;
            string library;
        };
        struct Actions : BaseActions {
            static auto remember(Reading, Id) -> Reference;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Unit_group : Group<Unit_group, Manager, Unit> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}

namespace fqsm::aspect {

template<>
struct Retrospection<rmmr::resource::Manager> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::Manager");
        d.one(field<&rmmr::resource::Manager::Quantum::location>("location"));
    }
};

template<>
struct Retrospection<rmmr::resource::Unit::Reference> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Reference");
        d.one(field<&rmmr::resource::Unit::Reference::id>("id"));
        d.one(field<&rmmr::resource::Unit::Reference::backup>("backup"));
    }
};

template<>
struct Retrospection<rmmr::resource::Unit> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::Unit");
        d.one(field<&rmmr::resource::Unit::Quantum::manager>("manager"));
        d.one(field<&rmmr::resource::Unit::Quantum::name>("name"));
        d.one(field<&rmmr::resource::Unit::Quantum::library>("library"));
    }
};

}
