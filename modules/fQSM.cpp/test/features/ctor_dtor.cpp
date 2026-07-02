#include "_common.h"

#include <map>
#include <string>

#include <fQSM/api/interface.h>

namespace local {
    using namespace fqsm::api;

    struct Stock : Entity<Stock> {
        struct Quantum {
            std::map<size_t, std::string> jobs;
        };
        using Reactions = DefaultReactions;
    };

    struct Worker : Entity<Worker> {
        struct Quantum {
            std::map<int, std::string> temp_field; // added to explore custom serialization with new system
        };

        using Reactions = DefaultReactions;
    };

    struct Management : Manager<Management, Stock, Worker> {
        struct Quantum {};
        struct Passport {
            int encoded;
        };
        using Reactions = DefaultReactions;
    };
}

namespace tests {

void ctor_dtor()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Stock>(),
        ask::schema::aspect<Worker>(),
        ask::schema::aspect<Management>(),
    });

    context::Realm main(schema);
}

} // namespace tests
