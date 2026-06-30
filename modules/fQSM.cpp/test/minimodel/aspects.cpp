#include "aspects.q1.h"

namespace tests::model {
    using namespace fqsm::api;

    // SomeEntity:
    struct SomeEntity::Actions::Private {
        static integer somePrivateFunc(integer val, integer mod) {
            return val % mod;
        }
    };
    auto SomeEntity::Actions::constantFunc(Reading context, Id id) -> integer {
        return Private::somePrivateFunc(get(context, id).value, global(context).modulus);
    }
}