#include "_common.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <fQSM/api/interface.h>

// Pins the structural behaviour of ask::schema::merge() when a schema is assembled from
// nested fragments (doctrine-file style: merge({ merge({...}), merge({...}) })) instead of
// one flat merge({...}). See modules/fQSM.cpp/interface/fQSM/manipulation/schema.h.
namespace {
    using namespace fqsm::api;

    // --- local test doctrine: Host entity + Cap feature (structural reactions) + an
    // unrelated Other entity, so fragments can be combined/nested in different shapes.

    struct Host : Entity<Host> {
        struct Quantum { integer value = 0; };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Cap : Feature<Cap, Host> {
        struct Quantum {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Other : Entity<Other> {
        struct Quantum { integer tag = 0; };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Entity with one custom reaction that counts its invocations.
    inline int countedCalls = 0;

    struct Counted : Entity<Counted> {
        struct Quantum { integer value = 0; };
        struct Internals : DefaultInternals {
            static void count(Reacting) { ++countedCalls; }
        };
        static const Behavior customAspectReactions() {
            return { reaction::aspect_wide<Counted>(&Internals::count) };
        }
    };

    // Feature requires its host to appear in the *same* patch as the new feature
    // (co-birth), so create+extend must happen as a single top-level call.
    auto spawn_host_with_cap(Writing context, integer value) -> Host::Id {
        const auto id = with<Host>::create(context, {.value = value});
        with<Cap>::extend(context, id, {});
        return id;
    }

    // --- schema signature helpers: turn a Schema into a small set of comparable,
    // deterministic strings/counts so structural equivalence (node set + reaction wiring)
    // can be asserted with EXPECT_EQ and any mismatch is readable in the failure message.

    std::string join_sorted(std::vector<std::string> items, std::string_view sep) {
        std::sort(items.begin(), items.end());
        std::string out;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i != 0) out += sep;
            out += items[i];
        }
        return out;
    }

    // Sorted list of the type names a single reaction listens() on, e.g. "Cap+Host".
    std::string listens_signature(const fqsm::features::reactions::Abstract& reaction) {
        std::vector<std::string> names;
        for (const auto& rtid : reaction.listens())
            names.emplace_back(fqsm::Rtid::name(rtid));
        return join_sorted(std::move(names), "+");
    }

    // All node names, sorted.
    std::string node_names_signature(const Schema& schema) {
        std::vector<std::string> names;
        for (const auto& [_, node] : schema->nodes)
            names.push_back(node.name);
        return join_sorted(std::move(names), ",");
    }

    // Per node: the sorted multiset of "listens_signature()" strings of its wired reactions.
    std::string wiring_signature(const Schema& schema) {
        std::vector<std::string> perNode;
        for (const auto& [_, node] : schema->nodes) {
            std::vector<std::string> sigs;
            for (const auto& reactionId : node.reactions)
                sigs.push_back(listens_signature(*schema->reactions[reactionId.raw()]));
            perNode.push_back(std::format("{}=[{}]", node.name, join_sorted(std::move(sigs), ";")));
        }
        return join_sorted(std::move(perNode), "|");
    }

    struct Signature final {
        std::size_t nodeCount = 0;
        std::size_t reactionCount = 0;
        std::string nodeNames;
        std::string wiring;
    };

    Signature signature_of(const Schema& schema) {
        return Signature{
            schema->nodes.size(),
            schema->reactions.size(),
            node_names_signature(schema),
            wiring_signature(schema),
        };
    }
}

namespace tests {

void schema_merge_nested_equals_flat()
{
    using namespace fqsm::api;

    const Schema flat = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
        ask::schema::aspect<Other>(),
    });

    // Non-trivial nesting: three fragments, one of them itself a merge (like two doctrine
    // files being pre-merged before joining the rest of the schema).
    const Schema nested = ask::schema::merge({
        ask::schema::merge({
            ask::schema::aspect<Host>(),
            ask::schema::aspect<Cap>(),
        }),
        ask::schema::aspect<Other>(),
    });

    const auto flatSig = signature_of(flat);
    const auto nestedSig = signature_of(nested);

    EXPECT_EQ(flatSig.nodeCount, nestedSig.nodeCount) << "same number of nodes";
    EXPECT_EQ(flatSig.nodeNames, nestedSig.nodeNames) << "same set of node names";
    EXPECT_EQ(flatSig.reactionCount, nestedSig.reactionCount) << "same total reaction count";
    EXPECT_EQ(flatSig.wiring, nestedSig.wiring) << "same per-node reaction wiring";
}

void schema_merge_order_independent()
{
    using namespace fqsm::api;

    const Schema ordered = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
        ask::schema::aspect<Other>(),
    });

    const Schema reordered = ask::schema::merge({
        ask::schema::aspect<Other>(),
        ask::schema::aspect<Cap>(),
        ask::schema::aspect<Host>(),
    });

    const auto a = signature_of(ordered);
    const auto b = signature_of(reordered);

    EXPECT_EQ(a.nodeCount, b.nodeCount) << "same number of nodes";
    EXPECT_EQ(a.nodeNames, b.nodeNames) << "merge order must not change the node set";
    EXPECT_EQ(a.reactionCount, b.reactionCount) << "same total reaction count";
    EXPECT_EQ(a.wiring, b.wiring) << "merge order must not change reaction wiring";
}

void schema_merge_single_fragment_identity()
{
    using namespace fqsm::api;

    const Schema fragment = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
    });

    const Schema wrapped = ask::schema::merge({ fragment });

    const auto a = signature_of(fragment);
    const auto b = signature_of(wrapped);

    EXPECT_EQ(a.nodeCount, b.nodeCount);
    EXPECT_EQ(a.nodeNames, b.nodeNames);
    EXPECT_EQ(a.reactionCount, b.reactionCount);
    EXPECT_EQ(a.wiring, b.wiring) << "merge({fragment}) must equal fragment";
}

// The same aspect registered by two fragments (e.g. two doctrine files both pull in the same aspect)
// is registered once: nodes, slots, structural rules and custom reactions are deduplicated together,
// because the reactions of a schema are the reactions of its descriptors.
void schema_merge_duplicate_aspect_registers_once()
{
    using namespace fqsm::api;

    const Schema once = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
        ask::schema::aspect<Counted>(),
    });

    const Schema twice = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
        ask::schema::aspect<Counted>(),
        ask::schema::aspect<Cap>(),     // duplicate registration of a feature (structural rules only)
        ask::schema::aspect<Counted>(), // duplicate registration of an aspect with a custom reaction
    });

    const auto a = signature_of(once);
    const auto b = signature_of(twice);

    EXPECT_EQ(a.nodeCount, b.nodeCount) << "nodes are deduplicated by type";
    EXPECT_EQ(a.nodeNames, b.nodeNames) << "node set unaffected by the duplicate registration";
    EXPECT_EQ(once->slotCount(), twice->slotCount()) << "slots are deduplicated";
    EXPECT_EQ(once->rules.size(), twice->rules.size()) << "structural rules are not duplicated";
    EXPECT_EQ(a.reactionCount, std::size_t{1});
    EXPECT_EQ(b.reactionCount, std::size_t{1}) << "custom reactions are not duplicated";
    EXPECT_EQ(a.wiring, b.wiring);

    const auto invocations = [](const Schema& schema) {
        establish::Realm main(schema);
        const auto id = with<Counted>::create(main, {1});
        countedCalls = 0;
        with<Counted>::modify(main, id)->value = 2;
        return countedCalls;
    };
    EXPECT_EQ(invocations(once), 1);
    EXPECT_EQ(invocations(twice), 1) << "a duplicated registration fires once per change";
}

void schema_merge_realm_feature_removal_nested_vs_flat()
{
    using namespace fqsm::api;

    const Schema flat = ask::schema::merge({
        ask::schema::aspect<Host>(),
        ask::schema::aspect<Cap>(),
    });

    const Schema nested = ask::schema::merge({
        ask::schema::merge({ ask::schema::aspect<Host>() }),
        ask::schema::merge({ ask::schema::aspect<Cap>() }),
    });

    for (const Schema& schema : { flat, nested }) {
        establish::Realm main(schema);

        const auto id = spawn_host_with_cap(main, 1);

        EXPECT_TRUE(main.result().good());
        EXPECT_TRUE(with<Host>::exists(main, id)) << "entity created";
        EXPECT_TRUE(with<Cap>::exists(main, id)) << "feature attached together with its host";

        with<Host>::remove(main, id);

        EXPECT_TRUE(main.result().good());
        EXPECT_FALSE(with<Host>::exists(main, id)) << "entity removed";
        EXPECT_FALSE(with<Cap>::exists(main, id))
            << "feature must be gone too (structural remove_with_parent reaction)";
    }
}

} // namespace tests
