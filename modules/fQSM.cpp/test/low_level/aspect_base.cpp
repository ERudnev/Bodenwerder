#include "_common.h"

#include <type_traits>

#include <fQSM/api/interface.h>

// Aspect base: each category exposes exactly its operations; a minimal declaration (Quantum only) is a full aspect.
namespace {
namespace local {
    using namespace fqsm::api;

    // minimal declarations: no Actions, no Internals, no customAspectReactions, no Global
    struct Node : Entity<Node> {
        struct Quantum { integer value; };
    };
    struct Light : Feature<Light, Node> {
        struct Quantum { integer color; };
    };
    struct Mark : Attribute<Mark, Node> {
        struct Quantum {};
    };
    struct Frame : Component<Frame, Node> {
        struct Quantum { integer size; };
    };
    struct Leaf : Entity<Leaf> {
        struct Quantum { integer value; };
    };
    struct Leaves : Group<Leaves, Node, Leaf> {};

    template<typename M> concept CanCreate = requires(Writing w, typename M::Quantum q) { with<M>::create(w, q); };
    template<typename M> concept CanExtend = requires(Writing w, typename M::Id id, typename M::Quantum q) { with<M>::extend(w, id, q); };
    template<typename M> concept CanExtendEmpty = requires(Writing w, typename M::Id id) { with<M>::extend(w, id); };
    template<typename M> concept CanKraken = requires(Writing w, typename M::Id id) { with<M>::kraken(w, id); };
    template<typename M> concept CanRemove = requires(Writing w, typename M::Id id) { with<M>::remove(w, id); };
    template<typename M> concept CanAddElement = requires(Writing w, typename M::Id id, Leaf::Quantum q) { with<M>::addElement(w, id, q); };
    template<typename M> concept CanDeleteElement = requires(Writing w, typename M::Id id, Leaf::Id e) { with<M>::deleteElement(w, id, e); };
    template<typename M> concept CanClear = requires(Writing w, typename M::Id id) { with<M>::clear(w, id); };
    template<typename M> concept CanRead = requires(Reading r, Writing w, typename M::Id id) {
        with<M>::count(r); with<M>::get(r, id); with<M>::find(r, id); with<M>::exists(r, id);
        with<M>::get_global(r); with<M>::modify(w, id); with<M>::modify_global(w);
    };

    template<typename M, bool create, bool extend, bool extendEmpty, bool kraken, bool remove, bool group>
    constexpr bool exposes =
        CanRead<M> and CanCreate<M> == create and CanExtend<M> == extend and CanExtendEmpty<M> == extendEmpty
        and CanKraken<M> == kraken and CanRemove<M> == remove
        and CanAddElement<M> == group and CanDeleteElement<M> == group and CanClear<M> == group;

    //                       create extend extend(id) kraken remove group
    static_assert(exposes<Node,   true,  false, false,     false, true,  false>);
    static_assert(exposes<Light,  false, true,  false,     true,  true,  false>);
    static_assert(exposes<Mark,   false, true,  false,     true,  true,  false>);
    static_assert(exposes<Frame,  false, true,  false,     true,  true,  false>);
    static_assert(exposes<Leaves, false, false, true,      true,  false, true>);

    // categories come from Traits
    static_assert(fqsm::category::Entity<Node> and fqsm::category::Standalone<Node>);
    static_assert(fqsm::category::Feature<Light> and fqsm::category::Parasitic<Light> and not fqsm::category::Attribute<Light>);
    static_assert(fqsm::category::Attribute<Mark> and fqsm::category::Component<Frame> and fqsm::category::Group<Leaves>);
    static_assert(std::is_same_v<Light::Id, Node::Id> and std::is_same_v<Leaves::Quantum, std::unordered_set<Leaf::Id>>);

    // defaults of a minimal declaration
    using Info = fqsm::meta::aspect_info<Light>;
    static_assert(std::is_same_v<with<Light>, fqsm::aspect::Capability<Light>>);
    static_assert(std::is_same_v<fqsm::GlobalValue<Light>, fqsm::meta::EmptyGlobal>);
    static_assert(std::is_same_v<Info::Internals, fqsm::meta::EmptyInternals>);
    static_assert(not Info::has_reactions and not Info::has_assemble);
}
} // namespace

namespace tests {

void aspect_minimal_declaration()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<Node>(),
        ask::schema::aspect<Light>(),
        ask::schema::aspect<Mark>(),
        ask::schema::aspect<Frame>(),
        ask::schema::aspect<Leaf>(),
        ask::schema::aspect<Leaves>(),
    });
    EXPECT_TRUE(schema->descriptors[schema->slotOf(fqsm::TypeId<Node>)].reactions.empty());

    establish::Realm main(schema);

    const auto spawn = [&](integer value) {
        return main.branch([&](Writing context) {
            const auto id = with<Node>::create(context, {value});
            with<Frame>::extend(context, id, {value * 10});
            with<Light>::extend(context, id, {value});
            with<Leaves>::extend(context, id);
            return id;
        });
    };
    const auto first = spawn(1);
    const auto second = spawn(2);
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<Node>::count(main), std::size_t{2});
    EXPECT_EQ(with<Light>::get(main, second).color, 2);

    with<Mark>::extend(main, first, {});
    const auto leaf = with<Leaves>::addElement(main, first, Leaf::Quantum{7});
    with<Leaves>::addElement(main, second, Leaf::Quantum{8});
    EXPECT_TRUE(main.result().good());
    EXPECT_EQ(with<Leaf>::get(main, leaf).value, 7);
    EXPECT_EQ(with<Leaves>::get(main, first).size(), std::size_t{1});
    EXPECT_EQ(with<Leaf>::count(main), std::size_t{2});

    // a node without its component is refused
    with<Node>::create(main.silent_work(), {3});
    EXPECT_FALSE(main.result().good());
    EXPECT_EQ(with<Node>::count(main), std::size_t{2});

    // removing the host cascades through every parasitic aspect and the group elements
    with<Node>::remove(main, first);
    EXPECT_TRUE(main.result().good());
    EXPECT_FALSE(with<Node>::exists(main, first));
    EXPECT_FALSE(with<Light>::exists(main, first));
    EXPECT_FALSE(with<Mark>::exists(main, first));
    EXPECT_FALSE(with<Frame>::exists(main, first));
    EXPECT_FALSE(with<Leaves>::exists(main, first));
    EXPECT_FALSE(with<Leaf>::exists(main, leaf));
    EXPECT_EQ(with<Leaf>::count(main), std::size_t{1});

    // kraken on a feature kills the whole aggregate
    with<Light>::kraken(main, second);
    EXPECT_EQ(with<Node>::count(main), std::size_t{0});
    EXPECT_EQ(with<Leaf>::count(main), std::size_t{0});
}

} // namespace tests
