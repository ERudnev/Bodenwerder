#include "_common.h"

#include <fQSM/api/interface.h>

namespace {

// this simulates C++ header file for real code
namespace local {
    using namespace fqsm::api;

    struct Keyring : Entity<Keyring> {
        using Password = string;
        struct Quantum {
            string name;
        };
        struct Global {
            integer generationCount = 0; // this is TOTAL count of ALL passwords generated throug ALL Keyrings
            integer activeCount = 0;
        };
        struct Actions : BaseActions {
            static auto generatePassword(Writing context, Id id)->Password {
                const auto& me = get(context, id);
                integer last = get_global(context).generationCount;
                integer active = get_global(context).activeCount;
                *modify_global(context) = Global{last + 1, active + 1};
                return me.name + std::to_string(last);
            }
            static void removePassword(Writing context, Id id, Password value) {
                auto globalCopy = get_global(context);
                --globalCopy.activeCount;
                *modify_global(context) = globalCopy;
            }
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Client : Entity<Client> {
        struct Quantum {
            Keyring::Id keyring;
            Keyring::Password password;
        };
        struct Internals : DefaultInternals {
            static void onDestroy(Writing context, Id, const Quantum& last) {
                with<Keyring>::removePassword(context, last.keyring, last.password);
            }
        };
        static const Behavior customAspectReactions() {
            return {
                reaction::deletion<Client>(&Internals::onDestroy),
            };
        }
    };

    struct Client_group : Group<Client_group, Keyring, Client> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct KeyManager : Archetype<KeyManager> {
        static void attachManager(Writing context, Keyring::Id keyring) {
            with<Client_group>::extend(context, keyring);
        }

        static auto addClient(Writing context, Keyring::Id keyring)->Client::Id {
            const auto newClient = with<Client_group>::addElement(
                context,
                keyring,
                {keyring, with<Keyring>::generatePassword(context, keyring)}
            );
            return newClient;
        }
    };
}

} // namespace

namespace tests {

void group_category_demo_scenario(fqsm::api::Schema schema)
{
    using namespace local;
    using namespace fqsm::api;

    establish::Realm main(schema);

    const auto primaryVault = with<Keyring>::create(main, {"vault-primary"});
    const auto backupVault = with<Keyring>::create(main, {"vault-backup"});

    with<KeyManager>::attachManager(main, primaryVault);
    with<KeyManager>::attachManager(main, backupVault);

    for (integer slot = 0; slot < 10; ++slot)
        with<KeyManager>::addClient(main, primaryVault);

    with<KeyManager>::addClient(main, backupVault);
    with<KeyManager>::addClient(main, backupVault);

    with<Keyring>::remove(main, primaryVault);

    EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 2);
}

void group_category()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Keyring>(),
        ask::schema::aspect<Client>(),
        ask::schema::aspect<Client_group>(),
    });

    group_category_demo_scenario(schema);

    {
        establish::Realm main(schema);

        const auto keyring = with<Keyring>::create(main, {"ring-A"});
        with<KeyManager>::attachManager(main, keyring);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Keyring>::exists(main, keyring));
        EXPECT_TRUE(with<Client_group>::exists(main, keyring));
        EXPECT_TRUE(with<Client_group>::get(main, keyring).empty());
        EXPECT_EQ(with<Keyring>::get_global(main).generationCount, 0);
        EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 0);
    }

    {
        establish::Realm main(schema);

        const auto keyring = with<Keyring>::create(main, {"ring-B"});
        with<KeyManager>::attachManager(main, keyring);

        const auto firstClient = with<KeyManager>::addClient(main, keyring);
        const auto secondClient = with<KeyManager>::addClient(main, keyring);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Client>::exists(main, firstClient));
        EXPECT_TRUE(with<Client>::exists(main, secondClient));

        const auto& group = with<Client_group>::get(main, keyring);
        EXPECT_EQ(group.size(), 2);
        EXPECT_TRUE(group.contains(firstClient));
        EXPECT_TRUE(group.contains(secondClient));

        const auto& firstState = with<Client>::get(main, firstClient);
        const auto& secondState = with<Client>::get(main, secondClient);
        EXPECT_EQ(firstState.keyring, keyring);
        EXPECT_EQ(secondState.keyring, keyring);
        EXPECT_EQ(firstState.password, "ring-B0");
        EXPECT_EQ(secondState.password, "ring-B1");

        EXPECT_EQ(with<Keyring>::get_global(main).generationCount, 2);
        EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 2);
    }

    {
        establish::Realm main(schema);

        const auto keyring = with<Keyring>::create(main, {"ring-C"});
        with<KeyManager>::attachManager(main, keyring);

        const auto firstClient = with<KeyManager>::addClient(main, keyring);
        const auto secondClient = with<KeyManager>::addClient(main, keyring);

        with<Client_group>::deleteElement(main, keyring, firstClient);

        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<Client>::exists(main, firstClient));
        EXPECT_TRUE(with<Client>::exists(main, secondClient));
        EXPECT_TRUE(with<Client_group>::get(main, keyring).contains(secondClient));
        EXPECT_FALSE(with<Client_group>::get(main, keyring).contains(firstClient));
        EXPECT_EQ(with<Keyring>::get_global(main).generationCount, 2);
        EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 1);

        with<Client_group>::remove(main, keyring);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Keyring>::exists(main, keyring));
        EXPECT_FALSE(with<Client_group>::exists(main, keyring));
        EXPECT_FALSE(with<Client>::exists(main, secondClient));
        EXPECT_EQ(with<Keyring>::get_global(main).generationCount, 2);
        EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 0);
    }
}

} // namespace tests
