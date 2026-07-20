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
        ask::schema::aspect<UselessItem>(),
        ask::schema::aspect<Person>(true),
        ask::schema::aspect<Family>(true),
    });

    establish::Realm main(schema);
    const auto dbPath = std::filesystem::path(DAQL_ASSETS_DIR) / "fqsm_workshop" / "registry.db";

    // Catalog still workshop-local; next migration step: archive slot on Schema::Node.
    fqsm_workshop::storage::DatabaseArchivist archivist({
        {fqsm::TypeId<Person>, fqsm_workshop::storage::ArchiveOps::of<Person>()},
        {fqsm::TypeId<Family>, fqsm_workshop::storage::ArchiveOps::of<Family>()},
    });

    fqsm::processing::Archivist::Palette palette;
    for (const auto& [type, node] : schema->nodes) {
        if (node.persistent)
            palette.insert(type);
    }

    const auto loadable = archivist.getTypesAtLocation(main, dbPath) & palette;
    if (loadable.empty()) {
        base::message("DB load failed, creating new registry");
        with<Registry>::createSixFamilies(main);
    } else {
        archivist.updateFromLocation(main, loadable, dbPath);
    }

    with<Person>::one_year_passed(main);

    archivist.saveToLocation(main, palette, dbPath);
}
