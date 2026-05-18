#include "_common.h"

#include <map>
#include <optional>
#include <string>

#include <base/containers/denseTable.h>
#include <base/containers/tableOverlay.h>
#include <base/containers/tableView.h>
#include <base/types/patches.h>

namespace tests {

void dense_table_overlay()
{
    using Table = base::DenseTable<int, std::string>;
    using View = base::TableView<int, std::string>;
    using Patch = base::types::Patch<std::string>;
    using PatchTable = base::DenseTable<int, Patch>;

    Table table;
    table.insert(1, "state");
    table.insert(2, "old");
    table.insert(3, "removed");

    PatchTable patch;
    patch.insert(2, Patch{"patched"});
    patch.insert(3, std::nullopt);
    patch.insert(4, Patch{"inserted"});

    base::TableOverlay<int, std::string> overlay(table, patch);

    const View& view = table;
    const View& overlayView = overlay;

    EXPECT_TRUE(view.contains(1));
    EXPECT_EQ(view.at(1), std::string{"state"});
    EXPECT_TRUE(patch.at(2).has_value());

    EXPECT_TRUE(overlayView.contains(1));
    EXPECT_TRUE(overlayView.contains(2));
    EXPECT_FALSE(overlayView.contains(3));
    EXPECT_TRUE(overlayView.contains(4));

    EXPECT_EQ(overlayView.at(1), std::string{"state"});
    EXPECT_EQ(overlayView.at(2), std::string{"patched"});
    EXPECT_EQ(overlayView.at(4), std::string{"inserted"});

    std::map<int, std::string> visible;

    for (const auto entry : overlayView) {
        visible.emplace(entry.first, entry.second);
    }

    EXPECT_EQ(visible.size(), std::size_t{3});
    EXPECT_EQ(visible.at(1), std::string{"state"});
    EXPECT_EQ(visible.at(2), std::string{"patched"});
    EXPECT_FALSE(visible.contains(3));
    EXPECT_EQ(visible.at(4), std::string{"inserted"});
}

} // namespace tests
