#include "_common.h"

#include <base/containers/table.h>
#include <base/containers/overlay.h>

#include <map>
#include <optional>
#include <string>

namespace tests {

void tableOverlay()
{
    using State = base::Table<int, int>;
    using PatchElement = base::patch::Element<int>;
    using Patch = base::Patch<int, PatchElement>;

    State state;
    state.insert(1, 10);
    state.insert(2, 20);
    state.insert(3, 30);

    Patch patch;
    patch.insert(2, 200);
    patch.insert(3, std::nullopt);
    patch.insert(4, 40);
    patch.insert(5, std::nullopt);

    base::Overlay<int, int> overlay(state, patch);

    EXPECT_EQ(overlay.size(), 3u);
    EXPECT_EQ(overlay.at(1), 10);
    EXPECT_EQ(overlay.at(2), 200);
    EXPECT_FALSE(overlay.contains(3));
    EXPECT_TRUE(overlay.contains(4));
    EXPECT_FALSE(overlay.contains(5));

    std::map<int, int> visible;
    const auto& view = overlay;
    for (const auto entry : view) {
        visible.emplace(entry.first, entry.second);
    }

    EXPECT_EQ(visible.size(), std::size_t{3});
    EXPECT_EQ(visible.at(1), 10);
    EXPECT_EQ(visible.at(2), 200);
    EXPECT_EQ(visible.at(4), 40);

    base::table::Write<int, int>& writable = overlay;
    writable.insert(1, 11);
    writable.erase(2);
    writable.erase(4);
    writable.insert(6, 60);

    EXPECT_EQ(overlay.size(), 2u);
    EXPECT_EQ(overlay.at(1), 11);
    EXPECT_FALSE(overlay.contains(2));
    EXPECT_FALSE(overlay.contains(4));
    EXPECT_TRUE(overlay.contains(6));
    EXPECT_EQ(overlay.at(6), 60);
    EXPECT_EQ(state.at(1), 10);
    EXPECT_EQ(state.at(2), 20);
    EXPECT_EQ(patch.at(1), PatchElement{11});
    EXPECT_FALSE(patch.at(2).has_value());
    EXPECT_TRUE(patch.contains(3));
    EXPECT_EQ(patch.at(6), PatchElement{60});

    std::map<int, std::string> changes;
    for (const auto entry : overlay.changes()) {
        if (!entry.second.has_value()) {
            changes.emplace(entry.first, "remove");
            continue;
        }

        changes.emplace(entry.first, std::to_string(*entry.second));
    }

    EXPECT_EQ(changes.size(), std::size_t{5});
    EXPECT_EQ(changes.at(1), std::string{"11"});
    EXPECT_EQ(changes.at(2), std::string{"remove"});
    EXPECT_EQ(changes.at(3), std::string{"remove"});
    EXPECT_EQ(changes.at(5), std::string{"remove"});
    EXPECT_EQ(changes.at(6), std::string{"60"});

    std::map<int, std::string> delta;
    for (const auto change : overlay.delta()) {
        if (change.add()) delta.emplace(change.key, "add");
        if (change.update()) delta.emplace(change.key, "update");
        if (change.remove()) delta.emplace(change.key, "remove");
    }

    EXPECT_EQ(delta.size(), std::size_t{4});
    EXPECT_EQ(delta.at(1), std::string{"update"});
    EXPECT_EQ(delta.at(2), std::string{"remove"});
    EXPECT_EQ(delta.at(3), std::string{"remove"});
    EXPECT_EQ(delta.at(6), std::string{"add"});
    EXPECT_FALSE(delta.contains(5));

    writable.clear();
    EXPECT_TRUE(overlay.empty());
    EXPECT_TRUE(state.contains(1));
    EXPECT_TRUE(state.contains(2));
    EXPECT_TRUE(state.contains(3));
}

} // namespace tests
