#pragma once

#include <memory>

#include <fQSM/api/interface.h>

#include "retrospection.h"

namespace placeholder {

    using namespace fqsm::api;

    struct Person : Entity<Person> {
        struct Quantum {
            string name;
            integer age;
            std::shared_ptr<string> cache;
        };

        struct Actions : BaseActions {
            static auto generate(Writing context, integer age) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
        static const Retrospection retrospection();
    };

    struct Family : Entity<Family> {
        struct Parents {
            Person::Id dad;
            Person::Id mom;
        };

        struct Quantum {
            string lastname;
            Parents parents;
            vector<Person::Id> children;
        };

        struct Global {
            integer sharedMoney = 0;
        };

        struct Actions : BaseActions {
            static auto generate(Writing context, bool dad, bool mom, integer children) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
        static const Retrospection retrospection();
    };

    struct Registry : Archetype<Registry> {
        static void createSixFamilies(Writing context);
    };

}
