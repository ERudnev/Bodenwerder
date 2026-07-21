#include "_common.h"

#include <filesystem>

#include <fQSM/api/interface.h>
#include <fQSM/processing/persistency/schema.h>
#include <pQRF/database/engine.h>

#include "features/domain/community.q1.h"

namespace tests {

void persistent_families()
{
    using namespace community;
    using namespace fqsm::api;
    namespace persist = fqsm::processing::persistency;
    namespace db = fqsm::processing::persistency::database;

    const Schema world = ask::schema::merge({
        ask::schema::aspect<UselessItem>(),
        ask::schema::aspect<Person>(),
        ask::schema::aspect<Family>(),
    });

    const persist::Schema archive = persist::merge({
        db::aspect<Person>(),
        db::aspect<Family>(),
    });

    const auto dbDir = std::filesystem::temp_directory_path() / "fqsm_persistent_families";
    std::filesystem::remove_all(dbDir);
    std::filesystem::create_directories(dbDir);
    const auto dbPath = dbDir / "registry.db";

    std::size_t personsAfterDay = 0;
    std::size_t familiesAfterDay = 0;
    integer sharedMoneyAfterDay = 0;
    Person::Id samplePerson = Person::Id::bad();
    integer sampleAgeAfterDay = 0;

    {
        establish::Realm origin(world);
        with<Registry>::createSixFamilies(origin);
        with<Person>::one_year_passed(origin);

        personsAfterDay = with<Person>::count(origin);
        familiesAfterDay = with<Family>::count(origin);
        sharedMoneyAfterDay = with<Family>::get_global(origin).sharedMoney;
        const auto sample = *origin->aspect<Person>().items().begin();
        samplePerson = sample.id;
        sampleAgeAfterDay = sample.value.age;

        EXPECT_EQ(with<UselessItem>::count(origin), 2) << "seed creates two UselessItems";
        EXPECT_EQ(familiesAfterDay, 6) << "six families";
        EXPECT_TRUE(personsAfterDay > 0) << "persons created with families";

        db::DatabaseArchivist archivist(archive);
        EXPECT_TRUE(archivist.saveToLocation(origin, archive->types(), dbPath)) << "save after +1 day";
    }

    {
        establish::Realm restored(world);
        EXPECT_EQ(with<Person>::count(restored), 0) << "second world starts empty";

        db::DatabaseArchivist archivist(archive);
        const auto palette = archive->types();
        const auto loadable = archivist.getTypesAtLocation(restored, dbPath) & palette;
        EXPECT_TRUE(!loadable.empty()) << "db has Person/Family tables";
        EXPECT_TRUE(archivist.updateFromLocation(restored, loadable, dbPath)) << "load into empty world";

        EXPECT_EQ(with<Person>::count(restored), personsAfterDay) << "person count matches origin";
        EXPECT_EQ(with<Family>::count(restored), familiesAfterDay) << "family count matches origin";
        EXPECT_EQ(with<UselessItem>::count(restored), 0) << "UselessItem not in persist schema";
        EXPECT_EQ(with<Family>::get_global(restored).sharedMoney, sharedMoneyAfterDay) << "sharedMoney matches origin";
        EXPECT_EQ(with<Person>::get(restored, samplePerson).age, sampleAgeAfterDay) << "aged person matches origin";
    }

    std::filesystem::remove_all(dbDir);
}

}
