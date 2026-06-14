#pragma once

namespace fqsm::state::slice {

    struct Erased {
        virtual ~Erased()=default;
    };
}