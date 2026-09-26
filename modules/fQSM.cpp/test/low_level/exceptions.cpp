#include "_common.h"

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fQSM/api/interface.h>

// An exception that leaves a change discards it: no partial commit during unwinding, no terminate
// from a destructor that normalizes.
namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // throws when a value becomes negative
    struct Fragile : Entity<Fragile> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals {
            static void check(Reacting context) {
                for (const auto& change : context.changes<Fragile>().updated())
                    if (change.now.value < 0) throw std::runtime_error("fragile: negative value");
            }
        };
        static const Behavior customAspectReactions() {
            return { reaction::aspect_wide<Fragile>(&Internals::check) };
        }
    };

    struct Failure : std::runtime_error {
        Failure() : std::runtime_error("worker failed") {}
    };

    bool mentions(const std::vector<std::string>& messages, std::string_view text) {
        for (const auto& message : messages)
            if (message.find(text) != std::string::npos) return true;
        return false;
    }
}
} // namespace

namespace tests {

void exceptions_branch_discards_on_unwind()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);

    bool caught = false;
    try {
        main.branch([](Writing context) {
            with<A>::create(context, {1});
            throw Failure();
        });
    } catch (const Failure&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
    EXPECT_EQ(with<A>::count(main), std::size_t{0}) << "the Branch left by an exception is discarded";

    main.branch([](Writing context) { with<A>::create(context, {2}); });
    EXPECT_EQ(with<A>::count(main), std::size_t{1}) << "a Branch that returns normally still merges";
}

void exceptions_session_discards_on_unwind()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);

    bool caught = false;
    try {
        Writing context = main;
        with<A>::create(context, {1});
        throw Failure();
    } catch (const Failure&) {
        caught = true;
    }
    EXPECT_TRUE(caught);
    EXPECT_EQ(with<A>::count(main), std::size_t{0}) << "the session left by an exception is discarded";
    EXPECT_FALSE(main.result().good()) << "the discard is reported";

    with<A>::create(main, {2});
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<A>::count(main), std::size_t{1});
}

void exceptions_reaction_throw_refuses()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<Fragile>();
    establish::Realm main(schema);
    const auto id = with<Fragile>::create(main, {1});
    EXPECT_TRUE(main.result().good());

    // the session closes in the destructor of an unnamed Writing: the reaction throws during normalization
    with<Fragile>::modify(main, id)->value = -1;
    EXPECT_FALSE(main.result().good()) << "a throwing reaction refuses the transaction";
    EXPECT_TRUE(mentions(main.result().critical, "fragile: negative value")) << "the message of the exception is kept";
    EXPECT_EQ(debug::read<Fragile>(main, id)->value, 1) << "nothing of the refused transaction is integrated";

    with<Fragile>::modify(main, id)->value = 3;
    EXPECT_TRUE(main.result().good()) << "the Realm works after the refusal";
    EXPECT_EQ(debug::read<Fragile>(main, id)->value, 3);
}

} // namespace tests
