#include "_common.h"

#include <format>
#include <vector>

#include <base/logging.h>
#include <fQSM/api/interface.h>

// Synthetic load workload, shaped like a resource install or a scene load: many composites in one branch.
//   refused    20,000 composites, then a refusal: the whole branch is discarded (the price of a rollback)
//   integrated the same 20,000, accepted and integrated
//   removed    2,000 of them in one transaction: the custody and category cascade, 3 waves
// The asserts pin the population; the timers are for comparison between commits on one machine.
namespace {
    using namespace fqsm::api;

    namespace load {
        struct Vec3 {
            double x = 0, y = 0, z = 0;
        };
        struct Root : Entity<Root> {
            struct Quantum {};
        };
        struct Node : Entity<Node> {
            struct Quantum {
                Vec3 position;
                float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            };
        };
        struct Node_group : Group<Node_group, Root, Node> {};
        struct Mesh : Feature<Mesh, Node> {
            struct Quantum {
                integer surfaces = 3;
                std::vector<integer> layers = {1, 2, 3};
            };
            static const Behavior customAspectReactions();
        };
        struct Body : Entity<Body> {
            struct Quantum {
                Vec3 position;
                Vec3 velocity;
                float mass = 1.0f;
            };
        };
        struct Thing : Entity<Thing> {
            struct Quantum {
                Custody<Body> body;
                Custody<Node> node;
                int64 bornAt = 0;
            };
            static const Behavior customAspectReactions();
        };
        struct Runtimes : Entity<Runtimes> {
            struct Quantum {};
            struct Global {
                integer released = 0;
            };
        };

        void release(Retrospecting context, Mesh::Id, const Mesh::Quantum&) {
            with<Runtimes>::modify_global(context)->released += 1;
        }
        auto Mesh::customAspectReactions() -> const Behavior {
            return {
                reaction::deletion<Mesh>(&release),
            };
        }
        auto Thing::customAspectReactions() -> const Behavior {
            return {
                reaction::structural::custody<Thing, Body, &Thing::Quantum::body>{},
                reaction::structural::custody<Thing, Node, &Thing::Quantum::node>{},
            };
        }

        Schema schema() {
            return ask::schema::merge({
                ask::schema::aspect<Root>(),
                ask::schema::aspect<Node>(),
                ask::schema::aspect<Node_group>(),
                ask::schema::aspect<Mesh>(),
                ask::schema::aspect<Body>(),
                ask::schema::aspect<Thing>(),
                ask::schema::aspect<Runtimes>(),
            });
        }

        Thing::Id spawn(Writing context, Root::Id root, double seed) {
            const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.position = {seed, 0.0, 0.0}});
            with<Mesh>::extend(context, node, {});
            const auto body = with<Body>::create(context, {.position = {seed, 0.0, 0.0}});
            return with<Thing>::create(context, {.body = body, .node = node});
        }

        constexpr int composites = 20000;
        constexpr int removed = 2000;
    }
}

namespace tests {

void workload_load()
{
    using namespace load;

    establish::Realm world(schema());
    const auto root = world.branch([&](Writing context) {
        const auto id = with<Root>::create(context, {});
        with<Node_group>::extend(context, id);
        return id;
    });
    EXPECT_TRUE(world.result().good());

    {
        testing::scoped_timer timer("workload_load: 20,000 composites in one branch, refused at the end");
        world.branch([&](Writing context) {
            for (int i = 0; i < composites; ++i)
                spawn(context, root, static_cast<double>(i));
            (void)context.refuse("workload_load: the load is refused on purpose");
        });
    }
    EXPECT_FALSE(world.result().good());
    EXPECT_EQ(with<Thing>::count(world), std::size_t{0});
    EXPECT_EQ(with<Node>::count(world), std::size_t{0});
    EXPECT_TRUE(with<Node_group>::get(world, root).empty());

    std::vector<Thing::Id> things;
    {
        testing::scoped_timer timer("workload_load: 20,000 composites in one branch, integrated");
        things = world.branch([&](Writing context) {
            std::vector<Thing::Id> made;
            made.reserve(composites);
            for (int i = 0; i < composites; ++i)
                made.push_back(spawn(context, root, static_cast<double>(i)));
            return made;
        });
    }
    EXPECT_TRUE(world.result().good());
    EXPECT_EQ(with<Thing>::count(world), static_cast<std::size_t>(composites));
    EXPECT_EQ(with<Mesh>::count(world), static_cast<std::size_t>(composites));
    EXPECT_EQ(with<Node_group>::get(world, root).size(), static_cast<std::size_t>(composites));

    {
        testing::scoped_timer timer("workload_load: remove 2,000 composites in one transaction (cascade)");
        Writing context = world;
        for (int i = 0; i < removed; ++i)
            with<Thing>::remove(context, things[static_cast<std::size_t>(i)]);
    }
    EXPECT_TRUE(world.result().good());
    EXPECT_EQ(with<Thing>::count(world), static_cast<std::size_t>(composites - removed));
    EXPECT_EQ(with<Body>::count(world), static_cast<std::size_t>(composites - removed));
    EXPECT_EQ(with<Node>::count(world), static_cast<std::size_t>(composites - removed));
    EXPECT_EQ(with<Node_group>::get(world, root).size(), static_cast<std::size_t>(composites - removed));
    EXPECT_EQ(with<Runtimes>::get_global(world).released, static_cast<integer>(removed));
}

} // namespace tests
