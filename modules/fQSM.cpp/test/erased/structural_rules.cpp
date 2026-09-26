#include "_common.h"

#include <stdexcept>
#include <string>

#include <fQSM/api/interface.h>
#include <fQSM/utility/messages.h>

// One direct test per structural rule derived from the aspect categories.
namespace {
namespace rules {
    using namespace fqsm::api;

    struct Host : Entity<Host> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Tag : Attribute<Tag, Host> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Limb : Feature<Limb, Host> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Hull : Entity<Hull> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Engine : Component<Engine, Hull> {
        struct Quantum { integer power; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Base : Entity<Base> {
        struct Quantum { integer value; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Member : Entity<Member> {
        struct Quantum { integer rank; };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Crew : Group<Crew, Base, Member> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Watch : Group<Watch, Base, Member> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    fqsm::Schema schema() {
        return ask::schema::merge({
            ask::schema::aspect<Host>(),
            ask::schema::aspect<Tag>(),
            ask::schema::aspect<Limb>(),
            ask::schema::aspect<Hull>(),
            ask::schema::aspect<Engine>(),
            ask::schema::aspect<Base>(),
            ask::schema::aspect<Member>(),
            ask::schema::aspect<Crew>(),
            ask::schema::aspect<Watch>(),
        });
    }

    Host::Id host_with_limb(establish::Realm& main) {
        return main.branch([](Writing context) {
            const auto id = with<Host>::create(context, {1});
            with<Limb>::extend(context, id, {2});
            return id;
        });
    }

    Base::Id base_with_groups(establish::Realm& main) {
        return main.branch([](Writing context) {
            const auto id = with<Base>::create(context, {1});
            with<Crew>::extend(context, id);
            with<Watch>::extend(context, id);
            return id;
        });
    }

    bool refused_with(const establish::Realm& main, const std::string& message) {
        for (const auto& critical : main.result().critical)
            if (critical == message) return true;
        return false;
    }
}
}

namespace tests {

void structural_remove_with_parent()
{
    using namespace rules;
    establish::Realm main(schema());
    const auto id = host_with_limb(main);
    const auto bystander = host_with_limb(main);
    with<Tag>::extend(main, id, {3});
    EXPECT_TRUE(with<Tag>::exists(main, id));

    with<Host>::remove(main, id);
    EXPECT_TRUE(with<Limb>::exists(main, bystander)) << "other hosts keep their parasitics";
    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<Host>::exists(main, id));
    EXPECT_FALSE(with<Limb>::exists(main, id));
    EXPECT_FALSE(with<Tag>::exists(main, id));
}

void structural_dead_parasitic_kills_parent()
{
    using namespace rules;
    establish::Realm main(schema());
    const auto id = host_with_limb(main);
    EXPECT_TRUE(with<Limb>::exists(main, id));

    const auto other = host_with_limb(main);
    main.branch([&](Writing context) { context.workers_interface().updates<Limb>().put_deletion(other); });
    EXPECT_FALSE(with<Limb>::exists(main, other));
    EXPECT_FALSE(with<Host>::exists(main, other)) << "a removed feature takes its host";
    EXPECT_TRUE(with<Host>::exists(main, id)) << "other hosts stay";
}

void structural_new_requires_existing_parent()
{
    using namespace rules;
    establish::Realm main(schema());
    const auto id = host_with_limb(main);

    with<Tag>::extend(main, id, {1});
    EXPECT_TRUE(main.result().good());
    EXPECT_TRUE(with<Tag>::exists(main, id)) << "attribute over an existing host";

    const auto missing = Host::Id::generate_random();
    with<Tag>::extend(main.silent_work(), missing, {1});
    EXPECT_FALSE(main.result().good());
    EXPECT_TRUE(refused_with(main, fqsm::utility::messages::structural_missing(
        fqsm::Rtid::name<Host>(), fqsm::Rtid::name<Tag>(), missing.raw())));
    EXPECT_FALSE(with<Tag>::exists(main, missing));
}

void structural_new_requires_parent_appears()
{
    using namespace rules;
    establish::Realm main(schema());
    const auto id = with<Host>::create(main, {1});
    EXPECT_TRUE(with<Host>::exists(main, id));

    with<Limb>::extend(main.silent_work(), id, {2});
    EXPECT_FALSE(main.result().good());
    EXPECT_TRUE(refused_with(main, fqsm::utility::messages::structural_same_patch(
        fqsm::Rtid::name<Host>(), fqsm::Rtid::name<Limb>(), id.raw())));
    EXPECT_FALSE(with<Limb>::exists(main, id));

    const auto born = host_with_limb(main);
    EXPECT_TRUE(main.result().good());
    EXPECT_TRUE(with<Limb>::exists(main, born)) << "host and feature born in one patch";
}

void structural_parent_appears_requires_component()
{
    using namespace rules;
    establish::Realm main(schema());

    const auto bare = with<Hull>::create(main.silent_work(), {1});
    EXPECT_FALSE(main.result().good());
    EXPECT_TRUE(refused_with(main, fqsm::utility::messages::structural_missing(
        fqsm::Rtid::name<Engine>(), fqsm::Rtid::name<Hull>(), bare.raw())));
    EXPECT_FALSE(with<Hull>::exists(main, bare));

    const auto whole = main.branch([](Writing context) {
        const auto id = with<Hull>::create(context, {1});
        with<Engine>::extend(context, id, {5});
        return id;
    });
    EXPECT_TRUE(main.result().good());
    EXPECT_TRUE(with<Engine>::exists(main, whole));
}

void structural_group_removal_removes_elements()
{
    using namespace rules;
    establish::Realm main(schema());
    const auto base = base_with_groups(main);
    const auto first = with<Crew>::addElement(main, base, Member::Quantum{1});
    const auto second = with<Crew>::addElement(main, base, Member::Quantum{2});
    EXPECT_EQ(with<Member>::count(main), std::size_t{2});

    const auto other = base_with_groups(main);
    const auto outsider = with<Crew>::addElement(main, other, Member::Quantum{3});

    main.branch([&](Writing context) { context.workers_interface().updates<Crew>().put_deletion(base); });
    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<Crew>::exists(main, base));
    EXPECT_FALSE(with<Member>::exists(main, first));
    EXPECT_FALSE(with<Member>::exists(main, second));
    EXPECT_TRUE(with<Member>::exists(main, outsider)) << "members of other groups stay";
}

void structural_element_removal_unhooks()
{
    using namespace rules;
    establish::Realm main(schema());
    const auto base = base_with_groups(main);
    const auto shared = with<Crew>::addElement(main, base, Member::Quantum{1});
    const auto kept = with<Crew>::addElement(main, base, Member::Quantum{2});
    main.branch([&](Writing context) {
        context.workers_interface().updates<Watch>().get_modification_access(base).insert(shared);
    });
    EXPECT_TRUE(with<Watch>::get(main, base).contains(shared));

    const auto third = with<Crew>::addElement(main, base, Member::Quantum{3});

    main.branch([&](Writing context) {
        with<Member>::remove(context, shared);
        with<Member>::remove(context, third);
    });
    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<Crew>::get(main, base).contains(shared));
    EXPECT_FALSE(with<Crew>::get(main, base).contains(third)) << "two removals in one Writing";
    EXPECT_FALSE(with<Watch>::get(main, base).contains(shared)) << "unhooked from every group holding it";
    EXPECT_TRUE(with<Crew>::get(main, base).contains(kept));
    EXPECT_EQ(with<Crew>::get(main, base).size(), std::size_t{1});
}

// A Feature registered without its Host: a fragment may be partial, but a Realm needs the host of every
// parasitic aspect, else the structural rules that keep the model consistent would be missing.
void structural_realm_requires_every_host()
{
    using namespace rules;
    fqsm::Schema lonely = ask::schema::aspect<Limb>();
    EXPECT_TRUE(lonely->rules.empty()) << "a partial fragment is allowed";

    std::string message;
    try {
        establish::Realm main(lonely);
    } catch (const std::logic_error& error) {
        message = error.what();
    }
    EXPECT_TRUE(message.find("Limb") != std::string::npos) << "the error names the aspect: " << message;
    EXPECT_TRUE(message.find("Host") != std::string::npos) << "the error names the missing host: " << message;

    fqsm::Schema whole = ask::schema::merge({ lonely, ask::schema::aspect<Host>() });
    establish::Realm main(whole);
    EXPECT_FALSE(whole->rules.empty());
}

} // namespace tests
