#include <Raidenmamare/controller/basic.q1.h>

#include <iQSM/api/_gateway_equal.h>

namespace rmmr::controller {

    void Core::Operations::update(Writing commit, seconds now_sec) {
        ops::global::modifier<Core>(commit)->clock = now_sec;
    }

    const Invariants Core::invariants{
        .structural = {},
        .logical = {},
    };
}
