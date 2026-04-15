#include <iQSM/helpers/schema.h>
#include <iQSM/helpers/world.h>
#include <iQSM/references.h>
#include <iQSM/resources/manager.h>

namespace iqsm::helpers::world {

    World create_no_resources(Schema schema) {
        const Schema empty_schema = iqsm::helpers::schema::assemble<>();
        auto manager = base::make_shared<iqsm::resources::ManagerCore>(empty_schema);
        return base::make_shared<const WorldObject>(std::move(schema), iqsm::freeze(manager));
    }

}
