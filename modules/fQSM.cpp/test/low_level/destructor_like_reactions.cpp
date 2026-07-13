#include "_common.h"

#include <fQSM/api/interface.h>

#include <base/logging.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            int value = 0;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct B : Component<B, A> {
        struct Quantum {};
        struct Internals : DefaultInternals {
            static void release(Writing context, Id id, const Quantum&) {
                const bool parent_exists = with<A>::exists(context, id);
                base::message(
                    "destructor_like_reactions: B::release id={} parent_exists={}",
                    id,
                    parent_exists);
                if (parent_exists) {
                    const auto& parent = with<A>::get(context, id);
                    base::message(
                        "destructor_like_reactions: parent value={}",
                        parent.value);
                } else {
                    base::message(
                        "destructor_like_reactions: parent is already missing");
                }
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::deletion<B>(&Internals::release),
            };
        }
    };

    struct Composition : Archetype<Composition> {
        static A::Id spawn(Writing context, int value) {
            const auto id = with<A>::create(context, {.value = value});
            with<B>::extend(context, id, {});
            return id;
        }
    };
}
} // namespace

namespace tests {

void destructor_like_reactions()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
        ask::schema::aspect<B>(),
    });

    establish::Realm main(schema);

    const auto id = with<Composition>::spawn(main, 42);


    base::message("destructor_like_reactions: removing parent A id={}", id);
    with<A>::remove(main, id);
}

} // namespace tests
