#include "_common.h"

#include <fQSM/api/interface.h>

namespace local {
    using namespace fqsm::api;

    struct Resource : Entity<Resource> {
        struct Quantum {
            std::string text;
        };
        using Reactions = DefaultReactions;
    };

    struct Handler : Entity<Handler> {
        struct Quantum {
            Resource::Id resourceId;
        };
        struct Actions : BaseActions {
            static void releaseResource(Writing context, Id, const Quantum& last) {
                with<Resource>::remove(context, last.resourceId);
            }
        };
        struct Reactions : BaseReactions {
            inline static const Behavior custom = {
                reaction::deletion<Handler>(&Actions::releaseResource),
            };
        };
    };
}

namespace tests {

void destructor()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Resource>(),
        ask::schema::aspect<Handler>(),
    });

    context::Realm main(schema);

    const auto resourceId = ask::item::create<Resource>(main, {"owned"});
    const auto handlerId = ask::item::create<Handler>(main, {.resourceId = resourceId});

    EXPECT_TRUE(ask::item::exists<Resource>(main, resourceId));
    EXPECT_TRUE(ask::item::exists<Handler>(main, handlerId));

    ask::item::update<Handler>(main, handlerId).remove();

    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(ask::item::exists<Handler>(main, handlerId));
    EXPECT_FALSE(ask::item::exists<Resource>(main, resourceId));
}

} // namespace tests
