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
    };

    struct UselessItem_group : Group<UselessItem_group, Person, UselessItem> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
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
    };

    struct Registry : Archetype<Registry> {
        static void createSixFamilies(Writing context);
        static auto grantPersonSomethingUseless(Writing context, Person::Id person, decltype(UselessItem::Quantum::reallyUseless) meaninglessValue) -> UselessItem::Id;
    };

}

namespace fqsm::aspect {

template<>
struct Retrospection<community::Person> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("community::Person");
        d.one(field<&community::Person::Quantum::name>("name"));
        d.one(field<&community::Person::Quantum::age>("age"));
    }
};

template<>
struct Retrospection<community::UselessItem_group> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("community::UselessItem_group");
        d.one(collection<community::UselessItem::Id>("members"));
    }
};

template<>
struct Retrospection<community::Family> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("community::Family");
        d.one(field<&community::Family::Quantum::lastname>("lastname"));
        d.one(field<&community::Family::Quantum::parents, &community::Family::Parents::dad>("parents.dad"));
        d.one(field<&community::Family::Quantum::parents, &community::Family::Parents::mom>("parents.mom"));
        d.one(collection<community::Person::Id, &community::Family::Quantum::children>("children"));
        d.all(field<&community::Family::Global::sharedMoney>("sharedMoney"));
        d.all(collection<std::string, &community::Family::Global::legends>("legends"));
    }
};

}

