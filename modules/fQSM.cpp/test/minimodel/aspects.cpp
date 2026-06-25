#include "aspects.q1.h"

namespace tests::model {
    using namespace fqsm::api;

    // SomeEntity:
    const SomeEntity::Behavior SomeEntity::custom{};

    struct SomeEntity::Actions::Private {
        static integer somePrivateFunc(integer val, integer mod) {
            return val % mod;
        }
    };
    auto SomeEntity::Actions::constantFunc(Reading context, Id id) -> integer {
        return Private::somePrivateFunc(get(context, id).value, global(context).modulus);
    }
}


namespace tests::model {
    using namespace fqsm::api;
    // SomeComponent:
    const SomeComponent::Behavior SomeComponent::custom{
        //.structural = {
        //}
    };
}

namespace tests::model {
    using namespace fqsm::api;

    // SecondaryAttribute:
    const SecondaryAttribute::Behavior SecondaryAttribute::custom{};

}