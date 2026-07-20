#include <fQSM/api/interface.h>

#include "placeholder.q1.h"

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
}
