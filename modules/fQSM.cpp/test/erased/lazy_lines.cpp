#include "_common.h"

#include <memory>

#include <fQSM/api/interface.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/pool.h>
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
            struct Global { integer total = 0; };
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
    auto patch = base::make_shared<fqsm::model::complex::Patch>(world);
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

void erased_pooled_lines()
{
    using namespace local;
    using Context = fqsm::processing::context::Operational;
    using Patch = fqsm::model::complex::Patch;

    const fqsm::Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });
    fqsm::model::complex::Reality world(schema);
    const auto slotB = schema->slotOf(fqsm::TypeId<B>);

    const auto write = [&](const std::shared_ptr<Patch>& target, integer first, integer second) {
        auto context = std::make_shared<Context>(world, base::shared_ref<Patch>(target), Context::Upstream{});
        fqsm::Writing writing{context};
        with<B>::create(writing, {first});
        if (second) with<B>::create(writing, {second});
        *with<B>::modify_global(writing) = B::Global{first};
    };

    auto first = std::make_shared<Patch>(world);
    write(first, 1, 2);
    EXPECT_EQ(first->linesCreated(), std::size_t{1});
    EXPECT_EQ(first->linesAllocated(), std::size_t{1});
    EXPECT_EQ(first->line(slotB)->count(), std::size_t{2});
    EXPECT_TRUE(first->line(slotB)->global() != nullptr);
    const auto allocatedBefore = world.linePool()->allocated();
    first.reset();

    auto second = std::make_shared<Patch>(world);
    {
        auto context = std::make_shared<Context>(world, base::shared_ref<Patch>(second), Context::Upstream{});
        fqsm::Writing writing{context};
        with<B>::create(writing, {3});
    }
    EXPECT_EQ(second->linesCreated(), std::size_t{1});
    EXPECT_EQ(second->linesAllocated(), std::size_t{0}) << "the second transaction reuses the pooled line";
    EXPECT_EQ(world.linePool()->allocated(), allocatedBefore) << "no new patch or future line";
    const auto* line = second->line(slotB);
    EXPECT_EQ(line->count(), std::size_t{1}) << "no stale patchlets from the first transaction";
    EXPECT_TRUE(line->global() == nullptr) << "no stale global from the first transaction";
    EXPECT_EQ(*static_cast<const integer*>(line->at(0).value), 3);
}

} // namespace tests
