#include "aspects.q1.h"

namespace tests::model {
    using namespace fqsm::api;

    // SomeEntity:
    const SomeEntity::Codex SomeEntity::codex{};

    struct SomeEntity_internals : SomeEntity::Service {
    };

    // SomeComponent:
    const SomeComponent::Codex SomeComponent::codex{
        //.structural = {
        //}
    };

    struct SomeComponent_internals : SomeComponent::Service {
    };

    // SecondaryAttribute:
    const SecondaryAttribute::Codex SecondaryAttribute::codex{};

    struct SecondaryAttribute_internals : SecondaryAttribute::Service {
    };


}