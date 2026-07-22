#include "_common.h"

#include <filesystem>

#include <base/logging.h>
#include <fQSM/api/interface.h>
#include <fQSM/processing/persistency/schema.h>
#include <pQRF/database/engine.h>
#include <pQRF/json/engine.h>

#include "features/domain/community.q1.h"

namespace tests {

void persistent_families()
{
    using namespace community;
    using namespace fqsm::api;
    namespace persist = fqsm::processing::persistency;
    namespace db = fqsm::processing::persistency::database;
    namespace js = fqsm::processing::persistency::json;

    const Schema world = ask::schema::merge({
        ask::schema::aspect<UselessItem>(),
        ask::schema::aspect<Person>(),
        ask::schema::aspect<UselessItem_group>(),
        ask::schema::aspect<Family>(),
    });

    const persist::Schema dbArchive = persist::merge({
        db::aspect<Person>(),
        db::aspect<UselessItem_group>(),
        db::aspect<Family>(),
    });
    const persist::Schema jsonArchive = persist::merge({
        js::aspect<Person>(),
        js::aspect<UselessItem_group>(),
        js::aspect<Family>(),
    });

    const auto workDir = std::filesystem::temp_directory_path() / "fqsm_persistent_families";
    std::filesystem::remove_all(workDir);
    std::filesystem::create_directories(workDir);
    const auto dbPath = workDir / "registry.db";
    const auto jsonPath = workDir / "registry.json";

    std::size_t personsAfterDay = 0;
    std::size_t familiesAfterDay = 0;
    integer sharedMoneyAfterDay = 0;
    maybe<Person::Id> witness;
    integer witnessAgeAfterDay = 0;

    {
        establish::Realm origin(world);
        with<Registry>::createSixFamilies(origin);
        witness = with<Person>::generate(origin, 99);
        with<Person>::one_year_passed(origin);

        personsAfterDay = with<Person>::count(origin);
        familiesAfterDay = with<Family>::count(origin);
        sharedMoneyAfterDay = with<Family>::get_global(origin).sharedMoney;
        witnessAgeAfterDay = with<Person>::get(origin, *witness).age;

        EXPECT_EQ(with<UselessItem>::count(origin), 2) << "seed creates two UselessItems";
        EXPECT_EQ(familiesAfterDay, 6) << "six families";
        EXPECT_TRUE(personsAfterDay > 0) << "persons created with families";

        db::DatabaseArchivist dbArchivist(dbArchive);
        EXPECT_TRUE(dbArchivist.saveToLocation(origin, dbArchive->types(), dbPath)) << "save sqlite";

        js::JsonArchivist jsonArchivist(jsonArchive);
        // temporary: peek at positional json dump
        //base::message("json snapshot:\n{}", jsonArchivist.to_string(origin, jsonArchive->types()));
        EXPECT_TRUE(jsonArchivist.saveToLocation(origin, jsonArchive->types(), jsonPath)) << "save json";
    }

    {
        establish::Realm fromDb(world);
        EXPECT_EQ(with<Person>::count(fromDb), 0) << "db world starts empty";

        db::DatabaseArchivist archivist(dbArchive);
        const auto palette = dbArchive->types();
        const auto loadable = archivist.getTypesAtLocation(fromDb, dbPath) & palette;
        EXPECT_TRUE(!loadable.empty()) << "db has Person/Family tables";
        {
            Stewarding session = fromDb;
            EXPECT_TRUE(archivist.replaceFromLocation(session, loadable, dbPath)) << "load sqlite";
        }

        EXPECT_EQ(with<Person>::count(fromDb), personsAfterDay) << "sqlite person count";
        EXPECT_EQ(with<Family>::count(fromDb), familiesAfterDay) << "sqlite family count";
        EXPECT_EQ(with<UselessItem>::count(fromDb), 0) << "UselessItem not in persist schema";
        EXPECT_EQ(with<Family>::get_global(fromDb).sharedMoney, sharedMoneyAfterDay) << "sqlite sharedMoney";
        EXPECT_EQ(with<Person>::get(fromDb, *witness).age, witnessAgeAfterDay) << "sqlite aged person";
    }

    {
        establish::Realm fromJson(world);
        EXPECT_EQ(with<Person>::count(fromJson), 0) << "json world starts empty";

        js::JsonArchivist archivist(jsonArchive);
        const auto palette = jsonArchive->types();
        const auto loadable = archivist.getTypesAtLocation(fromJson, jsonPath) & palette;
        EXPECT_TRUE(!loadable.empty()) << "json has Person/Family sections";
        {
            Stewarding session = fromJson;
            EXPECT_TRUE(archivist.replaceFromLocation(session, loadable, jsonPath)) << "load json";
        }

        EXPECT_EQ(with<Person>::count(fromJson), personsAfterDay) << "json person count";
        EXPECT_EQ(with<Family>::count(fromJson), familiesAfterDay) << "json family count";
        EXPECT_EQ(with<UselessItem>::count(fromJson), 0) << "UselessItem not in persist schema";
        EXPECT_EQ(with<Family>::get_global(fromJson).sharedMoney, sharedMoneyAfterDay) << "json sharedMoney";
        EXPECT_EQ(with<Person>::get(fromJson, *witness).age, witnessAgeAfterDay) << "json aged person";
    }

    std::filesystem::remove_all(workDir);
}

}
