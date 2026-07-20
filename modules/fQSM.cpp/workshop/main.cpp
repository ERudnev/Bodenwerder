#include <filesystem>

#include <fQSM/api/interface.h>

#include "placeholder.q1.h"
#include "storage.h"

using namespace fqsm::api;

int main()
{
    base::message("workshop starts...");
    using namespace placeholder;
    static const Schema schema = ask::schema::merge({
        ask::schema::aspect<Person>(),
        ask::schema::aspect<Family>(),
    });

    establish::Realm main(schema);
    with<Registry>::createSixFamilies(main);

    const auto dbPath = std::filesystem::path(DAQL_ASSETS_DIR) / "fqsm_workshop" / "registry.db";
    saveRegistry(main, dbPath);
}
