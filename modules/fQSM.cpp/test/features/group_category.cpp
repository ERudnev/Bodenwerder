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

    struct Tag : Attribute<Tag, Client> {
        struct Quantum {};
        using Internals = DefaultInternals;
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Client_group : Group<Client_group, Keyring, Client> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Tag_group : Group<Tag_group, Client_group, Tag> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct KeyManager : Archetype<KeyManager> {
        static Keyring::Id
        create_keyring(Writing context, Keyring::Quantum val) {
            const auto id = with<Keyring>::create(context, val);
            with<Client_group>::extend(context, id);
            return id;
        };

        static Client::Id
        addClient(Writing context, Keyring::Id keyring) {
            const auto newClient = with<Client_group>::addElement(
                context,
                keyring,
                {keyring, with<Keyring>::generatePassword(context, keyring)}
            );
            return newClient;
        }
    };

    struct TaggedKeyManager : Archetype<TaggedKeyManager> {
        static Keyring::Id
        create_keyring_tagged(Writing context, Keyring::Quantum val) {
            const auto id = KeyManager::create_keyring(context, val);
            with<Tag_group>::extend(context, id);
            return id;
        }

        static Client::Id
        addTaggedClient(Writing context, Keyring::Id keyring, Tag::Quantum tagValue) {
            const auto client = KeyManager::addClient(context, keyring);
            with<Tag_group>::addElement(context, keyring, client, tagValue);
            return client;
        }
    };
}

} // namespace

namespace tests {

using namespace local;
using namespace fqsm::api;

void group_category_demo_scenario(fqsm::api::Schema schema)
{
    establish::Realm main(schema);

    const auto primaryVault = with<KeyManager>::create_keyring(main, {"vault-primary"});
    const auto backupVault = with<KeyManager>::create_keyring(main, {"vault-backup"});

    for (integer slot = 0; slot < 10; ++slot)
        with<KeyManager>::addClient(main, primaryVault);

    with<KeyManager>::addClient(main, backupVault);
    with<KeyManager>::addClient(main, backupVault);

    with<Keyring>::remove(main, primaryVault);

    EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 2);
}

void group_category_group_hierarchy_scenario(fqsm::api::Schema schema)
{
    establish::Realm main(schema);
    const auto vault = with<TaggedKeyManager>::create_keyring_tagged(main, {"vault"});
    const auto taggedClient = with<TaggedKeyManager>::addTaggedClient(main, vault, {});
    const auto plainClient = with<KeyManager>::addClient(main, vault);

    EXPECT_TRUE(main.result().good());
    EXPECT_TRUE(with<Tag>::exists(main, taggedClient));
    EXPECT_FALSE(with<Tag>::exists(main, plainClient));
    EXPECT_TRUE(with<Tag_group>::exists(main, vault));
    EXPECT_TRUE(with<Tag_group>::get(main, vault).contains(taggedClient));
    EXPECT_FALSE(with<Tag_group>::get(main, vault).contains(plainClient));
}

void group_category_attribute_group_krakens(fqsm::api::Schema schema)
{
    establish::Realm main(schema);

    const auto vault = with<TaggedKeyManager>::create_keyring_tagged(main, {"vault"});
    const auto client = with<TaggedKeyManager>::addTaggedClient(main, vault, {});

    with<Tag_group>::deleteElement(main, vault, client);

    EXPECT_FALSE(with<Client>::exists(main, client));
}

void group_category_ring_scenarios(fqsm::api::Schema schema)
{
    {
        establish::Realm main(schema);

        const auto keyring = with<KeyManager>::create_keyring(main, {"ring-A"});

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Keyring>::exists(main, keyring));
        EXPECT_TRUE(with<Client_group>::exists(main, keyring));
        EXPECT_TRUE(with<Client_group>::get(main, keyring).empty());
        EXPECT_EQ(with<Keyring>::get_global(main).generationCount, 0);
        EXPECT_EQ(with<Keyring>::get_global(main).activeCount, 0);
    }

    {
        establish::Realm main(schema);

        const auto keyring = with<KeyManager>::create_keyring(main, {"ring-B"});

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

        const auto keyring = with<KeyManager>::create_keyring(main, {"ring-C"});

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


void group_category()
{
    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Keyring>(),
        ask::schema::aspect<Client>(),
        ask::schema::aspect<Client_group>(),
    });

    const Schema schema_extended = ask::schema::merge({
        ask::schema::aspect<Keyring>(),
        ask::schema::aspect<Client>(),
        ask::schema::aspect<Client_group>(),
        ask::schema::aspect<Tag>(),
        ask::schema::aspect<Tag_group>(),
    });

    group_category_demo_scenario(schema);
    group_category_group_hierarchy_scenario(schema_extended);
    group_category_attribute_group_krakens(schema_extended);
    group_category_ring_scenarios(schema);

}

} // namespace tests
