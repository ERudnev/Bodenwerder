#include "_common.h"
#include "minimodel/aspects.q1.h"

#include <map>

#include <base/shared_reference.h>
#include <fQSM/model/complex/draft.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/reality.h>

namespace tests {

void dense_table_overlay()
{
    using namespace ::tests::model;
    using Id = fqsm::Id<SomeEntity>;
    using Quantum = fqsm::Quantum<SomeEntity>;

    const fqsm::Schema schema = fqsm::manipulation::schema::aspect<SomeEntity>();

    fqsm::model::complex::Reality state(schema);
    state.aspect<SomeEntity>().items().insert(Id{1}, Quantum{1});
    state.aspect<SomeEntity>().items().insert(Id{2}, Quantum{2});
    state.aspect<SomeEntity>().items().insert(Id{3}, Quantum{3});

    auto patch = base::make_shared<fqsm::model::complex::Patch>(schema);
    patch->aspect<SomeEntity>().items.insert(Id{2}, Quantum{20});
    patch->aspect<SomeEntity>().items.insert(Id{3}, std::nullopt);
    patch->aspect<SomeEntity>().items.insert(Id{4}, Quantum{40});
    patch->aspect<SomeEntity>().items.insert(Id{5}, std::nullopt);

    fqsm::model::complex::Draft preview(state, patch);

    const auto& view = state.aspect<SomeEntity>().items();
    const auto& overlayView = preview.aspect<SomeEntity>().items();

    EXPECT_TRUE(view.contains(Id{1}));
    EXPECT_EQ(view.at(Id{1}).value, 1);
    EXPECT_TRUE(patch->aspect<SomeEntity>().items.at(Id{2}).has_value());

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
        visible.emplace(entry.key.raw(), entry.value.value);
    }

    EXPECT_EQ(visible.size(), std::size_t{3});
    EXPECT_EQ(visible.at(1), 1);
    EXPECT_EQ(visible.at(2), 20);
    EXPECT_FALSE(visible.contains(3));
    EXPECT_EQ(visible.at(4), 40);
    EXPECT_FALSE(visible.contains(5));

    std::map<int, std::string> delta;

    for (const auto change : preview.delta<SomeEntity>().get()) {
        if (change.add()) delta.emplace(change.key.raw(), "add");
        if (change.update()) delta.emplace(change.key.raw(), "update");
        if (change.remove()) delta.emplace(change.key.raw(), "remove");
    }

    EXPECT_EQ(delta.size(), std::size_t{3});
    EXPECT_EQ(delta.at(2), std::string{"update"});
    EXPECT_EQ(delta.at(3), std::string{"remove"});
    EXPECT_EQ(delta.at(4), std::string{"add"});
    EXPECT_FALSE(delta.contains(5));
}

} // namespace tests
