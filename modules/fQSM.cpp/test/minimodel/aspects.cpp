#include "aspects.q1.h"

namespace tests::model {
    using namespace fqsm::api;

    // SomeEntity:
    const SomeEntity::Codex SomeEntity::codex{};

    struct SomeEntity::Capabilities::Private {
        static integer somePrivateFunc(integer val, integer mod) {
            return val % mod;
        }
    };
    auto SomeEntity::Capabilities::constantFunc(Reading context, Id id) -> integer {
        // this version of "user code" is not final, not effective and must not be an example
        auto self = ask::item::get<SomeEntity>(context, id);
        auto global = ask::global::get<SomeEntity>(context);
        return Private::somePrivateFunc(self->value, global.modulus);
    }
}


namespace tests::model {
    using namespace fqsm::api;
    // SomeComponent:
    const SomeComponent::Codex SomeComponent::codex{
        //.structural = {
        //}
    };
}

namespace tests::model {
    using namespace fqsm::api;

    // SecondaryAttribute:
    const SecondaryAttribute::Codex SecondaryAttribute::codex{};

}