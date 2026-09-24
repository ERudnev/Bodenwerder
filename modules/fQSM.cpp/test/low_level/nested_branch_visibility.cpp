#include "_common.h"

#include <fQSM/api/interface.h>

#include <set>

// Nested establish::Branch (Branch-over-Branch-over-Realm) is exactly the
// Future-over-Future-over-Table nesting the cannonball cursor layer must handle:
// each Branch level is one more overlaid patch layer on the aspect's items().
namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };
}
} // namespace

namespace tests {

void nested_branch_meta_visibility()
{
    using namespace local;
    using namespace fqsm::api;
    using Id = fqsm::Id<A>;

    const Schema schema = ask::schema::aspect<A>();

    establish::Realm main(schema);
    const Id preexisting1 = with<A>::create(main, {.value = 1});
    const Id preexisting2 = with<A>::create(main, {.value = 2});

    establish::Branch outer(main);
    const Id idA = with<A>::create(outer, {.value = 100});    // A: created in outer
    with<A>::modify(outer, preexisting1)->value = 20;          // B: modified in outer

    establish::Branch inner(outer);
    const Id idC = with<A>::create(inner, {.value = 300});     // C: created in inner
    with<A>::modify(inner, idA)->value = 111;                  // A modified again, in inner

    const auto visible = [](fqsm::Reading view) {
        std::set<Id> ids;
        for (const auto entry : view->aspect<A>().items())
            ids.insert(entry.id);
        return ids;
    };

    // Inside the inner branch: pre-existing entities, plus A (inner's value), B (outer's
    // value, untouched by inner), plus C — exactly the layered-cursor visibility rule.
    EXPECT_TRUE(visible(inner) == std::set<Id>({preexisting1, preexisting2, idA, idC}));
    EXPECT_EQ(debug::count<A>(inner), std::size_t{4});

    EXPECT_EQ(debug::read<A>(inner, idA)->value, 111);
    EXPECT_EQ(debug::read<A>(inner, preexisting1)->value, 20);
    EXPECT_EQ(debug::read<A>(inner, idC)->value, 300);
    EXPECT_EQ(debug::read<A>(inner, preexisting2)->value, 2);
}

} // namespace tests
