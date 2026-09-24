#include "_common.h"

#include <optional>
#include <type_traits>

#include <fQSM/api/interface.h>

// Contexts: one Session per open change, thin handles over it.
namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum { integer value; };
        struct Global { integer seenAtDeletion = -1; };
        struct Internals : DefaultInternals {
            // the deletion is not visible from the last stable state
            static void onRemoved(Retrospecting context, Id id, const Quantum&) {
                const bool visible = with<A>::exists(context, id);
                with<A>::modify_global(context)->seenAtDeletion = visible ? 1 : 0;
            }
        };
        static const Behavior customAspectReactions() {
            return { reaction::deletion<A>(&Internals::onRemoved) };
        }
    };

    // a function signature says what the function may do
    static_assert(std::is_convertible_v<Writing, Reading> and not std::is_convertible_v<Reading, Writing>);
    static_assert(std::is_convertible_v<Stewarding, Writing> and std::is_convertible_v<Stewarding, fqsm::Direct<A>>);
    static_assert(not std::is_convertible_v<Writing, Stewarding> and not std::is_convertible_v<Writing, fqsm::Direct<A>>);
    static_assert(std::is_convertible_v<Reacting, Reading> and std::is_convertible_v<Reacting, Writing>);
    static_assert(not std::is_convertible_v<Reacting, Stewarding> and not std::is_convertible_v<fqsm::Retrospecting, Stewarding>);
    static_assert(not std::is_convertible_v<fqsm::Retrospecting, fqsm::Direct<A>> and not std::is_convertible_v<Reading, fqsm::Retrospecting>);
}
} // namespace

namespace tests {

void contexts_writing_copies_share_session()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);

    {
        std::optional<Writing> first(main);
        Writing second = *first;
        Writing third = second;
        with<A>::create(second, {1});
        with<A>::create(third, {2});
        EXPECT_EQ(with<A>::count(*first), std::size_t{2}) << "every copy writes into the one session";
        first.reset();
        EXPECT_EQ(with<A>::count(main), std::size_t{0}) << "the session stays open while a copy lives";
        EXPECT_EQ(with<A>::count(third), std::size_t{2});
    }
    EXPECT_EQ(with<A>::count(main), std::size_t{2}) << "the last copy closes the session: the Realm accepts it";
    EXPECT_TRUE(main.result().good());
}

void contexts_nested_branch_refusal()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);

    std::optional<A::Id> kept, refused;
    {
        establish::Branch outer(main);
        kept = with<A>::create(outer, {1});
        {
            establish::Branch inner(outer);
            refused = with<A>::create(inner, {2});
            static_cast<Writing>(inner).refuse("inner branch gives up");
            EXPECT_EQ(with<A>::count(inner), std::size_t{2});
        }
        EXPECT_EQ(with<A>::count(outer), std::size_t{1}) << "a refused inner branch is discarded";
    }
    EXPECT_TRUE(main.result().good()) << "the outer branch is not refused";
    EXPECT_TRUE(with<A>::exists(main, *kept));
    EXPECT_FALSE(with<A>::exists(main, *refused));
    EXPECT_EQ(main.result().warning.size(), std::size_t{1}) << "the refusal stays visible as a warning";
}

void contexts_stewarding_direct_and_writing()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);
    const auto first = with<A>::create(main, {1});
    const auto second = with<A>::create(main, {2});

    {
        Stewarding session = main;
        auto direct = session.direct<A>();
        direct.items.find(first)->value = 10;
        with<A>::modify(session, second)->value = 20;
        EXPECT_EQ(with<A>::get(session, first).value, 10) << "the session view sees the direct write";
        EXPECT_EQ(with<A>::get(session, second).value, 20);
        EXPECT_EQ(with<A>::get(main, second).value, 2) << "the Writing part waits for the session end";
    }
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<A>::get(main, first).value, 10);
    EXPECT_EQ(with<A>::get(main, second).value, 20);
}

void contexts_retrospecting_reads_origin()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);
    const auto id = with<A>::create(main, {1});

    with<A>::remove(main, id);
    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<A>::exists(main, id));
    EXPECT_EQ(with<A>::get_global(main).seenAtDeletion, 1) << "Retrospecting reads the last stable state";
}

} // namespace tests
