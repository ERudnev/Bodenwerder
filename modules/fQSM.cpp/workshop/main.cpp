#include <fQSM/api/interface.h>

#include "placeholder.q1.h"

// working with "placeholder" (syntax polisihng for *.q1.h / *.q1.cpp files
namespace tests {

    using namespace fqsm::api;

    void schema_world_from_etalon()
    {
        static const Schema schema = ask::schema::merge({
            ask::schema::aspect<placeholder::MyEntity>(),
        });

        context::Realm main(schema);

    }

} // namespace tests