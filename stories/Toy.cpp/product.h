#pragma once

#include <rmmr/wrapper/product.h>

#include "assets/library.h"

namespace toy {

    using namespace fqsm::api;

    class Product : public rmmr::wrapper::Product {
    public:
        void bindShared(const assets::Handles& handles) { shared = &handles; }

    protected:
        const assets::Handles* shared = nullptr;
    };

}
