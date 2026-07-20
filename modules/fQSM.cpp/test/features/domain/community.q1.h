#pragma once

#include <memory>

#include <fQSM/api/interface.h>
#include <fQSM/processing/persistency/database/retrospection.h>

namespace community {

    using namespace fqsm::api;
    using fqsm::processing::persistency::database::Retrospection;

    struct UselessItem : Entity<UselessItem> {
        struct Quantum {
            bool reallyUseless;
        };

        struct Actions : BaseActions {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Person : Entity<Person> {
        struct Quantum {
            string name;
            integer age;
            std::shared_ptr<string> cache;
        };

        struct Actions : BaseActions {
            static auto generate(Writing context, integer age) -> Id;
            static void one_year_passed(Writing context);
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
            vector<string> legends;
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
