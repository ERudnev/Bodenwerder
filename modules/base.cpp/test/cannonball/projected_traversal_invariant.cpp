#include "_common.h"

#include <base/cannonball/table.h>
#include <base/cannonball/draft.h>
#include <base/cannonball/patch.h>
#include <base/cannonball/delta/operational.h>

#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace tests {

// Pre-refactor anchor:
// traversal order may change, but completeness and reported meaning must not.
void projected_traversal_invariant()
{
    using Table = base::cannonball::Table<int, int>;
    using Patch = base::cannonball::Patch<int, int>;
    using Draft = base::cannonball::Draft<int, int>;
    using Delta = base::cannonball::delta::Operational<int, int>;

    Table state;
    state.insert(1, 10);
    state.insert(2, 20);
    state.insert(3, 30);
    state.insert(5, 50);

    Patch patch;
    patch.modify(2, 200);
    patch.insert(3, std::nullopt);
    patch.modify(4, 40);
    patch.modify(5, 55);

    Draft preview(state, patch);
    Draft draft(state, patch);
    Delta delta(state, patch);

    const std::map<int, int> expectedVisible{
        {1, 10},
        {2, 200},
        {4, 40},
        {5, 55},
    };

    std::map<int, int> previewVisible;
    for (const auto entry : preview)
        previewVisible.emplace(entry.id, entry.value);

    std::map<int, int> draftVisible;
    for (const auto entry : draft)
        draftVisible.emplace(entry.id, entry.value);

    EXPECT_TRUE(previewVisible == expectedVisible);
    EXPECT_TRUE(draftVisible == expectedVisible);
    EXPECT_EQ(preview.size(), expectedVisible.size());
    EXPECT_EQ(draft.size(), expectedVisible.size());

    for (const auto [id, value] : expectedVisible) {
        EXPECT_TRUE(preview.contains(id));
        EXPECT_TRUE(draft.contains(id));
        EXPECT_EQ(preview.at(id), value);
        EXPECT_EQ(draft.at(id), value);
    }

    EXPECT_FALSE(preview.contains(3));
    EXPECT_FALSE(draft.contains(3));

    using ChangeSnapshot = std::tuple<bool, std::optional<int>, std::optional<int>>;
    std::map<int, ChangeSnapshot> allChanges;
    for (const auto change : delta) {
        std::optional<int> before = std::nullopt;
        if (change.before.has_value()) {
            if (change.before.value()) before = *change.before.value();
            else before = std::optional<int>{};
        }
        const std::optional<int> after = change.after ? std::optional<int>{*change.after} : std::nullopt;
        allChanges.emplace(change.id, ChangeSnapshot{change.before.has_value(), before, after});
    }

    const std::map<int, ChangeSnapshot> expectedChanges{
        {2, {true, {20}, {200}}},
        {3, {true, {30}, std::nullopt}},
        {4, {true, std::optional<int>{}, {40}}},
        {5, {true, {50}, {55}}},
    };

    EXPECT_TRUE(allChanges == expectedChanges);

    std::set<int> added;
    for (const auto change : delta.added())
        added.insert(change.id);

    std::set<int> addedOrUpdated;
    for (const auto change : delta.addedOrUpdated())
        addedOrUpdated.insert(change.id);

    std::set<int> removed;
    for (const auto change : delta.removed())
        removed.insert(change.id);

    std::set<int> updated;
    for (const auto change : delta.updated())
        updated.insert(change.id);

    const std::set<int> expectedAdded{4};
    const std::set<int> expectedAddedOrUpdated{2, 4, 5};
    const std::set<int> expectedRemoved{3};
    const std::set<int> expectedUpdated{2, 5};

    EXPECT_TRUE(added == expectedAdded);
    EXPECT_TRUE(addedOrUpdated == expectedAddedOrUpdated);
    EXPECT_TRUE(removed == expectedRemoved);
    EXPECT_TRUE(updated == expectedUpdated);
}

} // namespace tests
