#include "_common.h"

#include <fQSM/api/interface.h>

namespace {

    using namespace fqsm::api;

    struct A : Entity<A> {
        struct Quantum {
            integer value;
            A::Id neighbor;
        };

        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }

        template<typename Desc>
        static void describe(Desc& d) {
            d.aspect("A");
            d.one(field<&Quantum::value>("value"));
            d.one(field<&Quantum::neighbor>("neighbor"));
        }
    };

}

namespace tests {

void remap_identities()
{
    base::message("incomplete test (and feature) - no identity remap yet");
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<A>(),
    });

    establish::Realm main(schema);
}

}
