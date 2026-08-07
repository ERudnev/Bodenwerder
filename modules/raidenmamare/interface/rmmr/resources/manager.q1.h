#pragma once

#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Unit : Entity<Unit> {
        struct Name {
            string library;
            string own;

            auto empty() const -> bool { return library.empty() and own.empty(); }
            auto text() const -> string { return library.empty() ? own : (library + "::" + own); }
            static auto from(string library, string own) -> Name { return Name{std::move(library), std::move(own)}; }
            friend auto operator<=>(const Name&, const Name&) = default;
        };

        struct Reference {
            Id id;
            Name backup;
        };

        struct Quantum {
            Name name;
        };

        struct Actions : BaseActions {
            static auto remember(Reading, Id) -> Reference;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Manager : Component<Manager, system::Core> {
        struct Quantum {
            filepath location;
        };
        struct Actions : BaseActions {
            static auto singleton(Reading) -> optional<Id>;
            static void load(Writing);
            // Q1: ?resolve(one<Unit>, filename)->filepath
            static auto resolve(Reading, const Unit::Quantum&, const filename&) -> filepath;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Unit_group : Group<Unit_group, Manager, Unit> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
