#include <rmmr/resources/manager.q1.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    auto Unit::Actions::remember(Reading context, Id id) -> Reference {
        return {.id = id, .backup = with<Unit>::get(context, id).name};
    }

}
