#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <map>
#include <optional>

#include <fQSM/model/complex/draft.h>
#include <fQSM/state/patch.h>
#include <fQSM/model/complex/data.h>

namespace tests {

void dense_table_overlay()
{
    using namespace ::tests::model;
    using Id = fqsm::Id<SomeEntity>;
    using Item = fqsm::state::slice::View<SomeEntity, fqsm::meta::axis::order::state>::Item;
    using PatchItem = fqsm::state::slice::View<SomeEntity, fqsm::meta::axis::order::patch>::Item;
    using View = base::TableView<Id, Item>;

    const fqsm::Schema schema = fqsm::manipulation::schema::aspect<SomeEntity>();

    fqsm::state::world::Data state(schema);
    state.items<SomeEntity>().insert(Id{1}, Item{1});
    state.items<SomeEntity>().insert(Id{2}, Item{2});
    state.items<SomeEntity>().insert(Id{3}, Item{3});

    fqsm::state::world::Patch patch(schema);
    patch.items<SomeEntity>().insert(Id{2}, PatchItem{Item{20}});
    patch.items<SomeEntity>().insert(Id{3}, PatchItem{std::nullopt});
    patch.items<SomeEntity>().insert(Id{4}, PatchItem{Item{40}});
    patch.items<SomeEntity>().insert(Id{5}, PatchItem{std::nullopt});

    fqsm::state::world::Draft preview(state, patch);

    const View& view = state.items<SomeEntity>();
    const View& overlayView = preview.items<SomeEntity>();

    EXPECT_TRUE(view.contains(Id{1}));
    EXPECT_EQ(view.at(Id{1}).value, 1);
    EXPECT_TRUE(patch.items<SomeEntity>().at(Id{2}).has_value());

    EXPECT_TRUE(overlayView.contains(Id{1}));
    EXPECT_TRUE(overlayView.contains(Id{2}));
    EXPECT_FALSE(overlayView.contains(Id{3}));
    EXPECT_TRUE(overlayView.contains(Id{4}));
    EXPECT_FALSE(overlayView.contains(Id{5}));

    EXPECT_EQ(overlayView.at(Id{1}).value, 1);
    EXPECT_EQ(overlayView.at(Id{2}).value, 20);
    EXPECT_EQ(overlayView.at(Id{4}).value, 40);

    std::map<int, int> visible;

    for (const auto entry : overlayView) {
        visible.emplace(entry.first.raw(), entry.second.value);
    }

    EXPECT_EQ(visible.size(), std::size_t{3});
    EXPECT_EQ(visible.at(1), 1);
    EXPECT_EQ(visible.at(2), 20);
    EXPECT_FALSE(visible.contains(3));
    EXPECT_EQ(visible.at(4), 40);
    EXPECT_FALSE(visible.contains(5));

    std::map<int, std::string> delta;

    for (const auto change : preview.delta<SomeEntity>()) {
        if (change.add()) delta.emplace(change.id.raw(), "add");
        if (change.update()) delta.emplace(change.id.raw(), "update");
        if (change.remove()) delta.emplace(change.id.raw(), "remove");
    }

    EXPECT_EQ(delta.size(), std::size_t{3});
    EXPECT_EQ(delta.at(2), std::string{"update"});
    EXPECT_EQ(delta.at(3), std::string{"remove"});
    EXPECT_EQ(delta.at(4), std::string{"add"});
    EXPECT_FALSE(delta.contains(5));
}

} // namespace tests
