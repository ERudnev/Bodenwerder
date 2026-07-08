#include "_common.h"

#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    struct Resource : Entity<Resource> {
        struct Quantum {
            std::string text;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Handler : Entity<Handler> {
        struct Quantum {
            Resource::Id resourceId;
        };
        struct Internals : DefaultInternals {
            static void releaseResource(Writing context, Id, const Quantum& last) {
                with<Resource>::remove(context, last.resourceId);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::deletion<Handler>(&Internals::releaseResource),
            };
        }
    };
}
} // namespace

namespace tests {

void destructor()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Resource>(),
        ask::schema::aspect<Handler>(),
    });

    establish::Realm main(schema);

    const auto resourceId = with<Resource>::create(main, {"owned"});
    const auto handlerId = with<Handler>::create(main, {.resourceId = resourceId});

    EXPECT_TRUE(with<Resource>::exists(main, resourceId));
    EXPECT_TRUE(with<Handler>::exists(main, handlerId));

    with<Handler>::remove(main, handlerId);

    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<Handler>::exists(main, handlerId));
    EXPECT_FALSE(with<Resource>::exists(main, resourceId));
}

} // namespace tests
