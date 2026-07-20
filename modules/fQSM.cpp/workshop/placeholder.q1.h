#pragma once

#include <fQSM/api/interface.h>

namespace placeholder {

    using namespace fqsm::api;

    struct Person : Entity<Person> {
        struct Quantum {
            string name;
            integer age;
        };

        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
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

        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Registry : Archetype<Registry> {
        static void createSixFamilies(Writing context);
    };

}
