#include "_common.h"

#include <fQSM/api/interface.h>

namespace {

    using namespace fqsm::api;

    template<fqsm::category::Any Meta>
    bool changes_hit(Reacting context) {
        // updated() only: bulk create must not feed the cascade (adds would depth-limit).
        for (const auto& change : context.changes<Meta>().updated()) {
            (void)change;
            return true;
        }
        return false;
    }

    // Fuel only from lower aspects. Self stays in aspect_wide listens, but writing
    // Self must not re-arm the same reaction forever.
    template<fqsm::category::Any Self, fqsm::category::Any... Below>
    void cascade_bump(Reacting context) {
        if (not (changes_hit<Below>(context) || ...)) {
            return;
        }
        for (const auto entry : context.proposal.aspect<Self>().items()) {
            auto quantum = entry.value;
            quantum.noise += 1;
            context.reaction<Self>().put_modification(entry.id, std::move(quantum));
            break;
        }
    }

    struct A : Entity<A> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct B : Entity<B> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<B, A>(&Internals::react),
            };
        }
    };

    struct C : Entity<C> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<C, B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<C, B, A>(&Internals::react),
            };
        }
    };

    struct D : Entity<D> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<D, C, B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<D, C, B, A>(&Internals::react),
            };
        }
    };

    struct E : Entity<E> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<E, D, C, B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<E, D, C, B, A>(&Internals::react),
            };
        }
    };

    struct F : Entity<F> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<F, E, D, C, B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<F, E, D, C, B, A>(&Internals::react),
            };
        }
    };

    struct G : Entity<G> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<G, F, E, D, C, B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<G, F, E, D, C, B, A>(&Internals::react),
            };
        }
    };

    struct H : Entity<H> {
        struct Quantum {
            integer noise = 0;
        };
        struct Internals : DefaultInternals {
            static void react(Reacting context) {
                cascade_bump<H, G, F, E, D, C, B, A>(context);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::aspect_wide<H, G, F, E, D, C, B, A>(&Internals::react),
            };
        }
    };

} // namespace

namespace tests {

    using namespace fqsm::api;

    constexpr integer per_aspect = 10000;
    constexpr int modify_rounds = 1000;

    void cascade_performance() {
        const Schema schema = ask::schema::merge({
            ask::schema::aspect<A>(),
            ask::schema::aspect<B>(),
            ask::schema::aspect<C>(),
            ask::schema::aspect<D>(),
            ask::schema::aspect<E>(),
            ask::schema::aspect<F>(),
            ask::schema::aspect<G>(),
            ask::schema::aspect<H>(),
        });

        establish::Realm main(schema);

        const auto pivot_a = main.branch([&](Writing context) {
            const auto create_label = std::format(
                "cascade_performance: create {} x A..H in one branch",
                per_aspect);
            testing::scoped_timer timer(create_label.c_str());
            A::Id first = with<A>::create(context, {});
            with<B>::create(context, {});
            with<C>::create(context, {});
            with<D>::create(context, {});
            with<E>::create(context, {});
            with<F>::create(context, {});
            with<G>::create(context, {});
            with<H>::create(context, {});
            for (integer index = 1; index < per_aspect; ++index) {
                with<A>::create(context, {});
                with<B>::create(context, {});
                with<C>::create(context, {});
                with<D>::create(context, {});
                with<E>::create(context, {});
                with<F>::create(context, {});
                with<G>::create(context, {});
                with<H>::create(context, {});
            }
            return first;
        });
        EXPECT_TRUE(main.result().good());
        EXPECT_EQ(with<A>::count(main), static_cast<std::size_t>(per_aspect));
        EXPECT_EQ(with<H>::count(main), static_cast<std::size_t>(per_aspect));

        {
            const auto modify_label = std::format(
                "cascade_performance: {}x modify A (separate writings)",
                modify_rounds);
            testing::scoped_timer timer(modify_label.c_str());
            for (int round = 0; round < modify_rounds; ++round) {
                with<A>::modify(main, pivot_a)->noise = round;
            }
        }
        EXPECT_TRUE(main.result().good());
        EXPECT_EQ(with<A>::get(main, pivot_a).noise, integer{modify_rounds - 1});
    }

} // namespace tests
