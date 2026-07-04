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
                integer last = global(context).generationCount;
                integer active = global(context).activeCount;
                global_set(context, {last + 1, active + 1});
                return me.name + std::to_string(last);
            }
            static void removePassword(Writing context, Id id, Password value) {
                auto globalCopy = global(context);
                --globalCopy.activeCount;
                global_set(context, globalCopy);
            }
        };
        using Reactions = DefaultReactions;
    };

    struct Client : Entity<Client> {
        struct Quantum {
            Keyring::Id keyring;
            Keyring::Password password;
        };
        struct Actions : BaseActions {
            static void onDestroy(Writing context, Id, const Quantum& last) {
                with<Keyring>::removePassword(context, last.keyring, last.password);
            }
        };
        struct Reactions : BaseReactions {
            inline static const Behavior custom = {
                reaction::deletion<Client>(&Actions::onDestroy),
            };
        };
    };

    struct Client_group : Group<Client_group, Keyring, Client> {};

    struct KeyManager : Archetype<KeyManager> {
        static void attachManager(Writing context, Keyring::Id keyring) {
            with<Client_group>::install(context, keyring);
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

void group_category()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Keyring>(),
        ask::schema::aspect<Client>(),
        ask::schema::aspect<Client_group>(),
    });

    {
        context::Realm main(schema);

        const auto keyring = ask::item::create<Keyring>(main, {"ring-A"});
        with<KeyManager>::attachManager(main, keyring);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(ask::item::exists<Keyring>(main, keyring));
        EXPECT_TRUE(ask::item::exists<Client_group>(main, keyring));
        EXPECT_TRUE(with<Client_group>::get(main, keyring).empty());
        EXPECT_EQ(with<Keyring>::global(main).generationCount, 0);
        EXPECT_EQ(with<Keyring>::global(main).activeCount, 0);
    }

    {
        context::Realm main(schema);

        const auto keyring = ask::item::create<Keyring>(main, {"ring-B"});
        with<KeyManager>::attachManager(main, keyring);

        const auto firstClient = with<KeyManager>::addClient(main, keyring);
        const auto secondClient = with<KeyManager>::addClient(main, keyring);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(ask::item::exists<Client>(main, firstClient));
        EXPECT_TRUE(ask::item::exists<Client>(main, secondClient));

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

        EXPECT_EQ(with<Keyring>::global(main).generationCount, 2);
        EXPECT_EQ(with<Keyring>::global(main).activeCount, 2);
    }

    {
        context::Realm main(schema);

        const auto keyring = ask::item::create<Keyring>(main, {"ring-C"});
        with<KeyManager>::attachManager(main, keyring);

        const auto firstClient = with<KeyManager>::addClient(main, keyring);
        const auto secondClient = with<KeyManager>::addClient(main, keyring);

        with<Client_group>::deleteElement(main, keyring, firstClient);

        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(ask::item::exists<Client>(main, firstClient));
        EXPECT_TRUE(ask::item::exists<Client>(main, secondClient));
        EXPECT_TRUE(with<Client_group>::get(main, keyring).contains(secondClient));
        EXPECT_FALSE(with<Client_group>::get(main, keyring).contains(firstClient));
        EXPECT_EQ(with<Keyring>::global(main).generationCount, 2);
        EXPECT_EQ(with<Keyring>::global(main).activeCount, 1);

        ask::item::update<Client_group>(main, keyring).remove();

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(ask::item::exists<Keyring>(main, keyring));
        EXPECT_FALSE(ask::item::exists<Client_group>(main, keyring));
        EXPECT_FALSE(ask::item::exists<Client>(main, secondClient));
        EXPECT_EQ(with<Keyring>::global(main).generationCount, 2);
        EXPECT_EQ(with<Keyring>::global(main).activeCount, 0);
    }
}

} // namespace tests
