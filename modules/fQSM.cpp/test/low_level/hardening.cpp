#include "_common.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include <fQSM/api/interface.h>
#include <fQSM/erased/line.h>

// Contracts that turn misuse into a compile error or a recoverable error, not into undefined behaviour.
namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // A handle refers to the session of the Branch; the Branch closes when branch() returns.
    struct ReturnsValue { integer operator()(Writing) const { return 1; } };
    struct ReturnsNothing { void operator()(Writing) const {} };
    struct ReturnsWriting { Writing operator()(Writing context) const { return context; } };

    template<typename Worker>
    concept Branchable = requires(establish::Realm& realm, Worker worker) { realm.branch(worker); };

    static_assert(Branchable<ReturnsValue> and Branchable<ReturnsNothing>);
    static_assert(not Branchable<ReturnsWriting>, "a handle must not escape the Branch that owns its session");

    // Nested Branches that close inner first, also when a failed expectation unwinds the test.
    struct Chain {
        std::vector<std::unique_ptr<establish::Branch>> branches;
        ~Chain() { while (not branches.empty()) branches.pop_back(); }
        establish::Branch& back() { return *branches.back(); }
        std::size_t size() const { return branches.size(); }
    };
}
} // namespace

namespace tests {

void contexts_branch_depth_is_checked()
{
    using namespace local;
    const Schema schema = ask::schema::aspect<A>();
    establish::Realm main(schema);
    with<A>::create(main, {1});

    // each nested Branch adds one layer to the cursor; the limit is checked when a Branch opens
    {
        Chain chain;
        chain.branches.push_back(std::make_unique<establish::Branch>(main));
        while (chain.size() < fqsm::erased::Cursor::MaxLayers)
            chain.branches.push_back(std::make_unique<establish::Branch>(chain.back()));

        EXPECT_EQ(debug::count<A>(chain.back()), std::size_t{1}) << "the deepest allowed Branch reads normally";

        bool refused = false;
        try {
            establish::Branch tooDeep(chain.back());
        } catch (const std::logic_error&) {
            refused = true;
        }
        EXPECT_TRUE(refused) << "a Branch past the layer limit is refused when it opens";
    }
    EXPECT_TRUE(main.result().good());
}

} // namespace tests
