#pragma once

#include <memory>

#include <fQSM/api/interface.h>

namespace community {

    using namespace fqsm::api;

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

        template<typename Desc>
        static void describe(Desc& d) {
            d.aspect("community::Person");
            d.one(field<&Quantum::name>("name"));
            d.one(field<&Quantum::age>("age"));
            // cache omitted = ignored
        }
    };

    struct Family : Entity<Family> {
        struct Parents {
            optional<Person::Id> dad;
            optional<Person::Id> mom;
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

        template<typename Desc>
        static void describe(Desc& d) {
            d.aspect("community::Family");
            d.one(field<&Quantum::lastname>("lastname"));
            d.one(field<&Quantum::parents, &Parents::dad>("parents.dad"));
            d.one(field<&Quantum::parents, &Parents::mom>("parents.mom"));
            d.one(collection<&Quantum::children>("children"));
            d.all(field<&Global::sharedMoney>("sharedMoney"));
            d.all(collection<&Global::legends>("legends"));
        }
    };

    struct Registry : Archetype<Registry> {
        static void createSixFamilies(Writing context);
    };

}
