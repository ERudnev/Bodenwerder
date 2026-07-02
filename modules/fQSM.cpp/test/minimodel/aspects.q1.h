#pragma once

#include <fQSM/api/interface.h>

// this is "generated" semantic-level domain code
namespace tests::model {
    using namespace fqsm::api;

    struct SomeEntity : Entity<SomeEntity> {
        struct Quantum {
            integer value;
        };
        struct Global {
            integer modulus = 2;
        };
        struct Actions : BaseActions {
            struct Private;
            static auto constantFunc(Reading, Id)->integer;
        };
        struct Reactions : BaseReactions {
            inline static const Behavior custom = {};
        };
    };

    struct SomeComponent : Component<SomeComponent, SomeEntity> {
        struct Quantum {
            string name;
        };
        struct Actions : BaseActions {};
        struct Reactions : BaseReactions {
            inline static const Behavior custom = {};
        };
    };

    struct SecondaryAttribute : Attribute<SecondaryAttribute, SomeComponent> {
        struct Quantum {
            integer attribute;
        };
        struct Actions : BaseActions {};
        struct Reactions : BaseReactions {
            inline static const Behavior custom = {};
        };
    };

    namespace archetype {
        struct EntWithComp : Archetype {
            struct Actions : BaseActions {
                static SomeEntity::Id spawn(Writing context, int value, string name) {
                    const auto id = ask::item::create<SomeEntity>(context, {value});
                    ask::item::create<SomeComponent>(context, id, {std::move(name)});
                    return id;
                }
            };
        };
    };
}
