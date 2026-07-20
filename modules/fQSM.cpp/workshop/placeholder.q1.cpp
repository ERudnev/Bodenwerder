#include "placeholder.q1.h"

namespace placeholder {

    using namespace fqsm::api;

    namespace {
        constexpr const char* person_names[] = {
            "Pavel",
            "Elena",
            "Marcus",
            "Sofia",
            "Leo",
            "Hiro",
            "Aya",
            "Ken",
            "Yuki",
            "Omar",
            "Layla",
            "Amir",
            "Noor",
            "Zara",
            "James",
            "Claire",
            "Emma",
            "Noah",
            "Olivia",
            "Liam",
            "Ivan",
            "Mila",
            "Marta",
            "Niko",
        };
    }

    auto Person::retrospection() -> const Retrospection {
        return {
            .aspectName = "placeholder::Person",
            .quanta = {
                .fields = {
                    Retrospection::column<string>({Retrospection::step<&Person::Quantum::name>("name")}),
                    Retrospection::column<integer>({Retrospection::step<&Person::Quantum::age>("age")}),
                    Retrospection::ignored({Retrospection::step<&Person::Quantum::cache>("cache")}),
                },
            },
        };
    }

    auto Family::retrospection() -> const Retrospection {
        return {
            .aspectName = "placeholder::Family",
            .quanta = {
                .fields = {
                    Retrospection::column<string>({Retrospection::step<&Family::Quantum::lastname>("lastname")}),
                    Retrospection::referenceColumn<Person::Id>("placeholder::Person", {
                        Retrospection::step<&Family::Quantum::parents>("parents"),
                        Retrospection::step<&Family::Parents::dad>("dad"),
                    }),
                    Retrospection::referenceColumn<Person::Id>("placeholder::Person", {
                        Retrospection::step<&Family::Quantum::parents>("parents"),
                        Retrospection::step<&Family::Parents::mom>("mom"),
                    }),
                },
            },
            .collections = {
                Retrospection::referenceCollection<vector<Person::Id>>(
                    "placeholder::Person",
                    {Retrospection::step<&Family::Quantum::children>("children")}
                ),
            },
            .globals = Retrospection::Globals{
                .fields = {
                    Retrospection::column<integer>({Retrospection::step<&Family::Global::sharedMoney>("sharedMoney")}),
                },
            },
        };
    }

    auto Person::Actions::generate(Writing context, integer age) -> Id {
        constexpr integer names_count = static_cast<integer>(sizeof(person_names) / sizeof(person_names[0]));
        const auto personIndex = static_cast<integer>(count(context));
        return create(context, {
            .name = std::string{person_names[personIndex % names_count]},
            .age = age,
            .cache = std::make_shared<string>(std::format("runtime-cache-{}", personIndex)),
        });
    }

    void Person::Actions::one_year_passed(Writing context) {
        for (const auto entry : context->aspect<Person>().items())
            ++modify(context, entry.id)->age;
    }

    auto Family::Actions::generate(Writing context, bool dad, bool mom, integer children) -> Id {
        const auto familyIndex = static_cast<integer>(count(context));
        const auto dadId = dad ? with<Person>::generate(context, 28 + familyIndex * 3) : Person::Id::bad();
        const auto momId = mom ? with<Person>::generate(context, 26 + familyIndex * 3) : Person::Id::bad();

        vector<Person::Id> childIds;
        for (integer childIndex = 0; childIndex < children; ++childIndex)
            childIds.push_back(with<Person>::generate(context, 4 + (familyIndex + childIndex * 3) % 14));

        string lastname = std::format("Household{}", familyIndex + 1);
        if (dad) lastname = with<Person>::get(context, dadId).name;
        else if (mom) lastname = with<Person>::get(context, momId).name;

        return create(context, {
            .lastname = std::move(lastname),
            .parents = {.dad = dadId, .mom = momId},
            .children = std::move(childIds),
        });
    }

    void Registry::createSixFamilies(Writing context) {
        with<Family>::generate(context, true, true, 0);
        with<Family>::generate(context, true, true, 1);
        with<Family>::generate(context, true, true, 2);
        with<Family>::generate(context, true, true, 3);
        with<Family>::generate(context, true, true, 4);
        with<Family>::generate(context, false, true, 0);

        with<Family>::modify_global(context)->sharedMoney = 15000;
    }

}
