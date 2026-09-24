#include "_common.h"

#include <memory>

#include <fQSM/api/interface.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/contexts/operational.h>

namespace {
    namespace local {
        using namespace fqsm::api;

        struct A : Entity<A> {
            struct Quantum { integer value; };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
        struct B : Entity<B> {
            struct Quantum { integer value; };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
        struct C : Entity<C> {
            struct Quantum { integer value; };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
    }
}

namespace tests {

void erased_lazy_patch_lines()
{
    using namespace local;
    using Context = fqsm::processing::context::Operational;

    const fqsm::Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
        ask::schema::aspect<C>(),
    });
    EXPECT_EQ(schema->slotCount(), std::size_t{3});
    EXPECT_EQ(schema->slotOf(fqsm::TypeId<B>), fqsm::model::intertype::Graph::Slot{1});

    fqsm::model::complex::Reality world(schema);
    auto patch = base::make_shared<fqsm::model::complex::Patch>(schema);
    {
        auto context = std::make_shared<Context>(world, patch, Context::Upstream{});
        fqsm::Writing writing{context};
        with<B>::create(writing, {1});
        with<B>::create(writing, {2});
    }
    EXPECT_EQ(patch->linesCreated(), std::size_t{1});
    EXPECT_TRUE(patch->line(1) != nullptr);
    EXPECT_TRUE(patch->line(0) == nullptr);
    EXPECT_EQ(patch->line(1)->count(), std::size_t{2});

    establish::Realm realm(schema);
    with<C>::create(realm, {3});
    EXPECT_EQ(with<C>::count(realm), std::size_t{1});
    EXPECT_EQ(with<A>::count(realm), std::size_t{0});
}

} // namespace tests
