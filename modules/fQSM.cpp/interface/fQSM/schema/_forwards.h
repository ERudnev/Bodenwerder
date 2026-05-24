#pragma once

#include <fQSM/references.h>

namespace fqsm::schema {
    struct Dag;
}

namespace fqsm {
    using Schema = fqsm::cref<schema::Dag>;
}
