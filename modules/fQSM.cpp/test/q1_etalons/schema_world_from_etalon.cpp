#include "_common.h"

//#include <Etalon.fqsm/aspects.q1.h>
#include <fQSM/api/interface.h>

// placeholder
namespace tests {
    void schema_world_from_etalon()
    {
    }
} // namespace tests

/*
namespace {
    using namespace fqsm::api;
    using namespace Q1_fQSM::Etalon;

    Schema schema_singleton() {
        static const Schema schema = ask::schema::merge({
            ask::schema::aspect<Trivia>(),
            ask::schema::aspect<SampleEntity>(),
            ask::schema::aspect<Tag>(),
            ask::schema::aspect<Remnant>(),
            ask::schema::aspect<SampleComponent>(),
            ask::schema::aspect<SampleAttribute>(),
            ask::schema::aspect<Note>(),
            ask::schema::aspect<Note_group>(),
        });

        return schema;
    }

    // small parts:
    void create_ten_samples_and_fibonacci_em(Schema schema) {
         establish::Realm main(schema);
    };

}

namespace tests {

void schema_world_from_etalon()
{
    create_ten_samples_and_fibonacci_em(schema_singleton());
}

} // namespace tests
*/
