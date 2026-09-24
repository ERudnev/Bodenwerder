#include "_common.h"

#include <format>

#include <base/logging.h>
#include <fQSM/api/interface.h>

// Mirrors the Eltanin composite: Thing <- Construct (custody of a Mesh actor and a Crystal body),
// Node <- Mesh <- MeshState, Body <- Crystal, Root <- Node_group of Node, and a deletion reaction on Mesh
// that writes (a resource release). Removing the Thing must take everything down. The waves it takes are pinned:
// the structural closure inside a wave makes the depth independent of the parasitic chain length; only the
// custody link (a registered reaction) and the release (another) still cost a wave each.
namespace {
    using namespace fqsm::api;

    namespace chain {
        struct Runtimes : Entity<Runtimes> {
            struct Quantum {};
            struct Global {
                integer released = 0;
            };
        };
        struct Root : Entity<Root> {
            struct Quantum {};
        };
        struct Node : Entity<Node> {
            struct Quantum {
                integer tag = 0;
            };
        };
        struct Node_group : Group<Node_group, Root, Node> {};
        struct Mesh : Feature<Mesh, Node> {
            struct Quantum {
                integer surfaces = 0;
            };
            static const Behavior customAspectReactions();
        };
        struct MeshState : Feature<MeshState, Mesh> {
            struct Quantum {};
        };
        struct Body : Entity<Body> {
            struct Quantum {};
        };
        struct Crystal : Feature<Crystal, Body> {
            struct Quantum {};
        };
        struct Thing : Entity<Thing> {
            struct Quantum {};
        };
        struct Construct : Feature<Construct, Thing> {
            struct Quantum {
                Custody<Crystal> body;
                Custody<Mesh> actor;
            };
            static const Behavior customAspectReactions();
        };

        void release(Retrospecting context, Mesh::Id, const Mesh::Quantum&) {
            with<Runtimes>::modify_global(context)->released += 1;
        }

        auto Mesh::customAspectReactions() -> const Behavior {
            return {
                reaction::deletion<Mesh>(&release),
            };
        }

        auto Construct::customAspectReactions() -> const Behavior {
            return {
                reaction::structural::custody<Construct, Crystal, &Construct::Quantum::body>{},
                reaction::structural::custody<Construct, Mesh, &Construct::Quantum::actor>{},
            };
        }
    }

    // a pure parasitic chain: no reactions, only category rules
    namespace deep {
        struct A : Entity<A> { struct Quantum {}; };
        struct B : Feature<B, A> { struct Quantum {}; };
        struct C : Feature<C, B> { struct Quantum {}; };
        struct D : Feature<D, C> { struct Quantum {}; };
        struct E : Feature<E, D> { struct Quantum {}; };
    }
}

namespace tests {

void cascade_closure()
{
    using namespace fqsm::api;

    { // the Eltanin-like composite
        using namespace chain;
        const Schema schema = ask::schema::merge({
            ask::schema::aspect<Runtimes>(),
            ask::schema::aspect<Root>(),
            ask::schema::aspect<Node>(),
            ask::schema::aspect<Node_group>(),
            ask::schema::aspect<Mesh>(),
            ask::schema::aspect<MeshState>(),
            ask::schema::aspect<Body>(),
            ask::schema::aspect<Crystal>(),
            ask::schema::aspect<Thing>(),
            ask::schema::aspect<Construct>(),
        });
        establish::Realm main(schema);

        struct Built { Root::Id root; Node::Id node; Body::Id body; Thing::Id thing; };
        const auto [root, node, body, thing] = main.branch([&](Writing context) {
            const auto root = with<Root>::create(context, {});
            with<Node_group>::extend(context, root);
            const auto node = with<Node_group>::addElement(context, root, Node::Quantum{.tag = 7});
            with<Mesh>::extend(context, node, {.surfaces = 3});
            with<MeshState>::extend(context, node, {});
            const auto body = with<Body>::create(context, {});
            with<Crystal>::extend(context, body, {});
            const auto thing = with<Thing>::create(context, {});
            with<Construct>::extend(context, thing, {.body = body, .actor = node});
            return Built{root, node, body, thing};
        });
        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Construct>::exists(main, thing));
        EXPECT_TRUE(with<Node_group>::get(main, root).contains(node));
        base::message(std::format("cascade_closure: composite build took {} waves", main.result().waves));

        with<Thing>::remove(main, thing);
        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<Thing>::exists(main, thing));
        EXPECT_FALSE(with<Construct>::exists(main, thing));
        EXPECT_FALSE(with<Mesh>::exists(main, node)) << "custody: the actor is buried with the construct";
        EXPECT_FALSE(with<MeshState>::exists(main, node)) << "feature of the buried mesh";
        EXPECT_FALSE(with<Node>::exists(main, node)) << "dead feature kills its node";
        EXPECT_FALSE(with<Crystal>::exists(main, body)) << "custody: the body is buried with the construct";
        EXPECT_FALSE(with<Body>::exists(main, body)) << "dead feature kills its body";
        EXPECT_TRUE(with<Root>::exists(main, root));
        EXPECT_TRUE(with<Node_group>::get(main, root).empty()) << "the removed node is unhooked from its group";
        EXPECT_TRUE(with<Runtimes>::get_global(main).released == 1) << "the mesh release ran once";

        const int waves = main.result().waves;
        base::message(std::format("cascade_closure: composite removal took {} waves", waves));
        // wave 1: Thing -> Construct (rule), custody buries Mesh and Crystal (reaction)
        // wave 2: Mesh -> MeshState, Node, unhook; Crystal -> Body (rules, one closure); release (reaction)
        // wave 3: the release wrote a global: nothing structural, no reaction -> stop
        EXPECT_TRUE(waves == 3) << "before the closure this took 5 waves";
    }

    { // a five-deep parasitic chain: one wave, whatever the depth
        using namespace deep;
        const Schema schema = ask::schema::merge({
            ask::schema::aspect<A>(),
            ask::schema::aspect<B>(),
            ask::schema::aspect<C>(),
            ask::schema::aspect<D>(),
            ask::schema::aspect<E>(),
        });
        establish::Realm main(schema);
        const auto build = [&](Writing context) {
            const auto a = with<A>::create(context, {});
            with<B>::extend(context, a, {});
            with<C>::extend(context, a, {});
            with<D>::extend(context, a, {});
            with<E>::extend(context, a, {});
            return a;
        };
        const auto a = main.branch(build);
        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<E>::exists(main, a));

        with<A>::remove(main, a);
        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<B>::exists(main, a));
        EXPECT_FALSE(with<E>::exists(main, a));
        const int waves = main.result().waves;
        base::message(std::format("cascade_closure: five-deep chain removal took {} waves", waves));
        EXPECT_TRUE(waves == 1) << "before the closure this took 5 waves";

        // and from the leaf up: kraken on E kills A and everything in between
        const auto again = main.branch(build);
        with<E>::kraken(main, again);
        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<A>::exists(main, again));
        EXPECT_FALSE(with<C>::exists(main, again));
        base::message(std::format("cascade_closure: five-deep kraken took {} waves", main.result().waves));
        EXPECT_TRUE(main.result().waves == 1);
    }
}

} // namespace tests
