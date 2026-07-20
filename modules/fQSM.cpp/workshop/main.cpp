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
    const auto dbPath = std::filesystem::path(DAQL_ASSETS_DIR) / "fqsm_workshop" / "registry.db";
    if (not loadRegistry(main, dbPath)) {
        base::message("DB load failed, creating new registry");
        with<Registry>::createSixFamilies(main);
    }

    with<Person>::one_year_passed(main);

    saveRegistry(main, dbPath);
}
