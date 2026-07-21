#include "_common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <fQSM/api/interface.h>

// Bootstrap: describe with one/all × field/collection; schema collect without SQLite.

namespace experimental {

    using namespace fqsm::api;

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

template<typename T>
struct sql_type {
    static consteval void require() {
        static_assert(sizeof(T) == 0, "no SQL type mapping for this leaf");
    }
};

template<>
struct sql_type<std::string> {
    static constexpr std::string_view name = "TEXT";
    static consteval void require() {}
};

template<>
struct sql_type<std::int32_t> {
    static constexpr std::string_view name = "INTEGER";
    static consteval void require() {}
};

template<>
struct sql_type<bool> {
    static constexpr std::string_view name = "INTEGER";
    static consteval void require() {}
};

template<typename Meta, typename Base>
struct sql_type<fqsm::Identifier<Meta, Base>> {
    static constexpr std::string_view name = "INTEGER";
    static consteval void require() {}
};

struct SchemaColumn {
    std::string_view name;
    std::string_view sqlType;
};

struct SchemaCollection {
    std::string_view name;
    std::string_view elementSqlType;
};

// Collects relational layout from Meta::describe — proof we can build schemas from the form.
template<typename Meta>
struct SchemaDesc {
    std::string_view aspectName{};
    std::vector<SchemaColumn> one_fields{};
    std::vector<SchemaCollection> one_collections{};
    std::vector<SchemaColumn> all_fields{};
    std::vector<SchemaCollection> all_collections{};

    void aspect(std::string_view name) { aspectName = name; }

    template<auto... Members>
    void one(fqsm::aspect::Field<Members...> slot) {
        using Leaf = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
        sql_type<Leaf>::require();
        one_fields.push_back({slot.name, sql_type<Leaf>::name});
    }

    template<auto... Members>
    void one(fqsm::aspect::Collection<Members...> slot) {
        using Container = std::decay_t<decltype(slot.get(std::declval<typename Meta::Quantum&>()))>;
        using Elem = typename Container::value_type;
        sql_type<Elem>::require();
        one_collections.push_back({slot.name, sql_type<Elem>::name});
    }

    template<auto... Members>
    void all(fqsm::aspect::Field<Members...> slot) {
        using Leaf = std::decay_t<decltype(slot.get(std::declval<fqsm::GlobalValue<Meta>&>()))>;
        sql_type<Leaf>::require();
        all_fields.push_back({slot.name, sql_type<Leaf>::name});
    }

    template<auto... Members>
    void all(fqsm::aspect::Collection<Members...> slot) {
        using Container = std::decay_t<decltype(slot.get(std::declval<fqsm::GlobalValue<Meta>&>()))>;
        using Elem = typename Container::value_type;
        sql_type<Elem>::require();
        all_collections.push_back({slot.name, sql_type<Elem>::name});
    }
};

template<typename Meta>
auto collect_schema() -> SchemaDesc<Meta> {
    SchemaDesc<Meta> schema{};
    Meta::describe(schema);
    return schema;
}

auto draft_quanta_ddl(std::string_view aspectName, const std::vector<SchemaColumn>& columns) -> std::string {
    std::string out = "CREATE TABLE \"";
    out += aspectName;
    out += "\" (\"id\" INTEGER PRIMARY KEY NOT NULL";
    for (const auto& column : columns) {
        out += ", \"";
        out += column.name;
        out += "\" ";
        out += column.sqlType;
        out += " NOT NULL";
    }
    out += ")";
    return out;
}

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

    static_assert(fqsm::meta::category::musthave::Retrospection<Person>);
    static_assert(fqsm::meta::category::musthave::Retrospection<Family>);
    static_assert(!fqsm::meta::category::musthave::Retrospection<UselessItem>);

    const auto personSchema = collect_schema<Person>();
    EXPECT_EQ(personSchema.aspectName, "experimental::Person");
    EXPECT_EQ(personSchema.one_fields.size(), 2u);
    EXPECT_EQ(personSchema.one_fields[0].name, "name");
    EXPECT_EQ(personSchema.one_fields[0].sqlType, "TEXT");
    EXPECT_EQ(personSchema.one_fields[1].name, "age");
    EXPECT_EQ(personSchema.one_fields[1].sqlType, "INTEGER");
    EXPECT_TRUE(personSchema.one_collections.empty());
    EXPECT_TRUE(personSchema.all_fields.empty());

    const auto personDdl = draft_quanta_ddl(personSchema.aspectName, personSchema.one_fields);
    EXPECT_EQ(
        personDdl,
        "CREATE TABLE \"experimental::Person\" (\"id\" INTEGER PRIMARY KEY NOT NULL"
        ", \"name\" TEXT NOT NULL, \"age\" INTEGER NOT NULL)"
    );

    const auto familySchema = collect_schema<Family>();
    EXPECT_EQ(familySchema.aspectName, "experimental::Family");
    EXPECT_EQ(familySchema.one_fields.size(), 3u);
    EXPECT_EQ(familySchema.one_fields[0].name, "lastname");
    EXPECT_EQ(familySchema.one_fields[0].sqlType, "TEXT");
    EXPECT_EQ(familySchema.one_fields[1].name, "parents.dad");
    EXPECT_EQ(familySchema.one_fields[1].sqlType, "INTEGER");
    EXPECT_EQ(familySchema.one_fields[2].name, "parents.mom");
    EXPECT_EQ(familySchema.one_fields[2].sqlType, "INTEGER");
    EXPECT_EQ(familySchema.one_collections.size(), 1u);
    EXPECT_EQ(familySchema.one_collections[0].name, "children");
    EXPECT_EQ(familySchema.one_collections[0].elementSqlType, "INTEGER");
    EXPECT_EQ(familySchema.all_fields.size(), 1u);
    EXPECT_EQ(familySchema.all_fields[0].name, "sharedMoney");
    EXPECT_EQ(familySchema.all_fields[0].sqlType, "INTEGER");
    EXPECT_EQ(familySchema.all_collections.size(), 1u);
    EXPECT_EQ(familySchema.all_collections[0].name, "legends");
    EXPECT_EQ(familySchema.all_collections[0].elementSqlType, "TEXT");

    const Schema world = ask::schema::merge({
        ask::schema::aspect<UselessItem>(),
        ask::schema::aspect<Person>(),
        ask::schema::aspect<Family>(),
    });
    establish::Realm realm(world);
    archivist_placeholder(realm);
}

}
