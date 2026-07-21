#include "_common.h"

#include <memory>

#include <fQSM/api/interface.h>
#include <fQSM/aspect/persistency.h>

// Bootstrap: describe with one/all × field/collection (Q1 vocabulary).

namespace experimental {

    using namespace fqsm::api;
    using fqsm::aspect::collection;
    using fqsm::aspect::field;

    struct UselessItem : Entity<UselessItem> {
        struct Quantum {
            bool reallyUseless;
        };

        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
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
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }

        template<typename Desc>
        static void describe(Desc& d) {
            d.aspect("experimental::Person");
            d.one(field<&Quantum::name>("name"));
            d.one(field<&Quantum::age>("age"));
            // cache omitted = ignored
        }
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
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }

        template<typename Desc>
        static void describe(Desc& d) {
            d.aspect("experimental::Family");
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

    auto Person::Actions::generate(Writing, integer) -> Id {
        _INCOMPLETE_;
    }

    void Person::Actions::one_year_passed(Writing) {
        _INCOMPLETE_;
    }

    auto Family::Actions::generate(Writing, bool, bool, integer) -> Id {
        _INCOMPLETE_;
    }

    void Registry::createSixFamilies(Writing) {
        _INCOMPLETE_;
    }

}

namespace tests {

namespace {

struct CountDesc {
    std::string_view aspectName{};
    std::size_t one_fields = 0;
    std::size_t one_collections = 0;
    std::size_t all_fields = 0;
    std::size_t all_collections = 0;

    void aspect(std::string_view name) { aspectName = name; }

    template<auto... Members>
    void one(fqsm::aspect::Field<Members...>) { ++one_fields; }

    template<auto... Members>
    void one(fqsm::aspect::Collection<Members...>) { ++one_collections; }

    template<auto... Members>
    void all(fqsm::aspect::Field<Members...>) { ++all_fields; }

    template<auto... Members>
    void all(fqsm::aspect::Collection<Members...>) { ++all_collections; }
};

template<typename Meta>
struct ArchiveDesc {
    fqsm::Writing context;

    void aspect(std::string_view) {}

    void one(auto slot) {
        using namespace fqsm::api;
        for (const auto entry : context->aspect<Meta>().items())
            (void)slot.get(entry.value);
    }

    void all(auto slot) {
        using namespace fqsm::api;
        (void)slot.get(with<Meta>::get_global(context));
    }
};

}

template<typename Meta>
void archive_aspect_stub(fqsm::Writing context)
{
    ArchiveDesc<Meta> desc{context};
    Meta::describe(desc);
}

void archivist_placeholder(fqsm::Writing context)
{
    using namespace experimental;
    archive_aspect_stub<Person>(context);
    archive_aspect_stub<Family>(context);
}

void temp_persistency()
{
    using namespace experimental;
    using namespace fqsm::api;

    static_assert(fqsm::aspect::HasRetrospection<Person>);
    static_assert(fqsm::aspect::HasRetrospection<Family>);
    static_assert(!fqsm::aspect::HasRetrospection<UselessItem>);

    CountDesc personCount{};
    Person::describe(personCount);
    CountDesc familyCount{};
    Family::describe(familyCount);

    EXPECT_EQ(personCount.aspectName, "experimental::Person");
    EXPECT_EQ(personCount.one_fields, 2u);
    EXPECT_EQ(familyCount.one_collections, 1u);
    EXPECT_EQ(familyCount.all_fields, 1u);

    const Schema world = ask::schema::merge({
        ask::schema::aspect<UselessItem>(),
        ask::schema::aspect<Person>(),
        ask::schema::aspect<Family>(),
    });
    establish::Realm realm(world);
    archivist_placeholder(realm);
}

}
