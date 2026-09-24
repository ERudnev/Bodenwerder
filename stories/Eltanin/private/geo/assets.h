#pragma once

#include <fQSM/api/interface.h>
#include <rmmr/wrapper/library.h>

namespace eltanin::geo::assets {

    using namespace fqsm::api;

    auto add(Writing, const rmmr::wrapper::assets::Handles&) -> bool;

}
