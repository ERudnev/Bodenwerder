#include "placeholder.q1.h"

namespace placeholder {

    using namespace fqsm::api;

    namespace {

        auto person(Writing context, string name, integer age) -> Person::Id {
            return with<Person>::create(context, {
                .name = std::move(name),
                .age = age,
            });
        }

        auto family(Writing context, string lastname, Person::Id dad, Person::Id mom, vector<Person::Id> children) -> Family::Id {
            return with<Family>::create(context, {
                .lastname = std::move(lastname),
                .parents = {.dad = dad, .mom = mom},
                .children = std::move(children),
            });
        }

    }

    void Registry::createSixFamilies(Writing context) {
        // 0 children — empty nest
        {
            const auto dad = person(context, "Pavel", 47);
            const auto mom = person(context, "Elena", 45);
            family(context, "Novak", dad, mom, {});
        }

        // 1 child
        {
            const auto dad = person(context, "Marcus", 34);
            const auto mom = person(context, "Sofia", 32);
            const auto child = person(context, "Leo", 6);
            family(context, "Berg", dad, mom, {child});
        }

        // 2 children
        {
            const auto dad = person(context, "Hiro", 41);
            const auto mom = person(context, "Aya", 38);
            family(context, "Sato", dad, mom, {
                person(context, "Ken", 12),
                person(context, "Yuki", 9),
            });
        }

        // 3 children
        {
            const auto dad = person(context, "Omar", 39);
            const auto mom = person(context, "Layla", 37);
            family(context, "Hassan", dad, mom, {
                person(context, "Amir", 14),
                person(context, "Noor", 11),
                person(context, "Zara", 4),
            });
        }

        // 4 children — full house
        {
            const auto dad = person(context, "James", 44);
            const auto mom = person(context, "Claire", 42);
            family(context, "Whitman", dad, mom, {
                person(context, "Emma", 16),
                person(context, "Noah", 13),
                person(context, "Olivia", 8),
                person(context, "Liam", 2),
            });
        }

        // 0 children again — young couple, not yet parents
        {
            const auto dad = person(context, "Ivan", 28);
            const auto mom = person(context, "Mila", 26);
            family(context, "Kovac", dad, mom, {});
        }

        with<Family>::modify_global(context)->sharedMoney = 15000;
    }

}
