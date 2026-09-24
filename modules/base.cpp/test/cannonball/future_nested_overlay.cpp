#include "_common.h"

#include <base/cannonball/table.h>
#include <base/cannonball/patch.h>
#include <base/cannonball/future.h>

#include <map>
#include <set>

namespace tests {

// Future-over-Future-over-Table: three overlaid patch layers on one root table, exercising
// every id-visibility case the layered Cursor must get right.
void future_nested_overlay()
{
    using Table = base::cannonball::Table<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Patchlet = base::cannonball::Patchlet<int>;
    using Future = base::cannonball::Future<int, int>;
    using Mode = base::cannonball::SeeChanges;

    Table table;
    table.insert(1, 10);
    table.insert(2, 20);
    table.insert(6, 60);
    table.insert(8, 80);

    Patch p1; // layer 0 (oldest)
    p1.insert(1, Patchlet::modification(11));  // root id modified at layer 0...
    p1.insert(2, Patchlet::deletion(20));      // root id tombstoned at layer 0...
    p1.insert(3, Patchlet::modification(30));  // patch-only, added at layer 0...
    p1.insert(4, Patchlet::modification(40));  // patch-only, added at layer 0...
    p1.insert(5, Patchlet::deletion(0));       // tombstone-only, id never in root or added

    Patch p2; // layer 1
    p2.insert(1, Patchlet::modification(12));  // ...and again at layer 1 (value from layer 1)
    p2.insert(2, Patchlet::modification(22));  // ...re-added at layer 1 (once, root phase)
    p2.insert(3, Patchlet::deletion(30));      // ...tombstoned at layer 1 (never visible)
    p2.insert(4, Patchlet::modification(44));  // ...modified at layer 1 (once, value from layer 1)
    p2.insert(5, Patchlet::modification(55));  // ...re-added at layer 1 (once, layer 1 phase)

    Patch p3; // layer 2 (exercises 3-deep nesting)
    p3.insert(7, Patchlet::modification(77));  // patch-only, introduced at the top layer
    p3.insert(8, Patchlet::deletion(80));      // root id, deleted only at the top layer

    Future advancing(table, p1, Mode::observable);
    Future proposal(advancing, p2, Mode::observable);
    Future review(proposal, p3, Mode::observable);

    // Brute-force model: apply patches sequentially over a plain map.
    std::map<int, int> model{{1, 10}, {2, 20}, {6, 60}, {8, 80}};
    const auto apply = [&](const Patch& patch) {
        for (const auto entry : patch) {
            if (entry.value.tombstone) model.erase(entry.id);
            else model[entry.id] = entry.value.quantum;
        }
    };
    apply(p1);
    apply(p2);
    apply(p3);

    const std::map<int, int> expected{
        {1, 12}, {2, 22}, {4, 44}, {5, 55}, {6, 60}, {7, 77},
    };
    EXPECT_TRUE(model == expected);

    std::map<int, int> observed;
    for (const auto entry : review)
        observed.emplace(entry.id, entry.value);

    EXPECT_TRUE(observed == model);
    EXPECT_EQ(review.size(), model.size());

    const std::set<int> mentioned{1, 2, 3, 4, 5, 6, 7, 8};
    for (const int id : mentioned) {
        const bool expectedVisible = model.contains(id);
        EXPECT_EQ(review.contains(id), expectedVisible);

        const auto* found = review.find(id);
        if (expectedVisible) {
            EXPECT_TRUE(found != nullptr);
            EXPECT_EQ(*found, model.at(id));
            EXPECT_EQ(review.at(id), model.at(id));
        } else {
            EXPECT_TRUE(found == nullptr);
        }
    }

    // Plain Table (zero layers) is unaffected by any of this.
    std::set<int> plainKeys;
    for (const auto entry : table) plainKeys.insert(entry.id);
    EXPECT_TRUE(plainKeys == std::set<int>({1, 2, 6, 8}));
    EXPECT_EQ(table.at(1), 10);
}

} // namespace tests
