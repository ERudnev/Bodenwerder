#include "_common.h"

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include <fQSM/erased/delta.h>
#include <fQSM/erased/line.h>
#include <fQSM/erased/future_line.h>

#include "counted.h"
#include "patch_stub.h"

namespace {
    struct NoDefault {
        explicit NoDefault(int v) : value(v) {}
        int value;
    };

    const int& as_int(const void* value) { return *static_cast<const int*>(value); }
}

namespace tests {

void erased_line_basic()
{
    using fqsm::erased::Line;
    Line line(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<int>());
    EXPECT_EQ(line.size(), std::size_t{0});
    EXPECT_TRUE(line.global() != nullptr);
    EXPECT_EQ(as_int(line.global()), 0);

    for (int i = 1; i <= 50; ++i) {
        const int value = i * 10;
        line.insert(static_cast<fqsm::RawId>(i), &value);
    }
    EXPECT_EQ(line.size(), std::size_t{50});
    EXPECT_TRUE(line.contains(7));
    EXPECT_FALSE(line.contains(51));
    EXPECT_EQ(as_int(line.find(7)), 70);
    EXPECT_TRUE(line.find(51) == nullptr);

    const int replaced = -7;
    line.insert(7, &replaced);
    EXPECT_EQ(line.size(), std::size_t{50});
    EXPECT_EQ(as_int(line.find(7)), -7);

    const auto* seven = line.find(7);
    line.insert(8, seven);
    EXPECT_EQ(as_int(line.find(8)), -7);

    std::map<fqsm::RawId, int> model;
    for (fqsm::RawId i = 1; i <= 50; ++i)
        model[i] = as_int(line.find(i));

    std::mt19937 random(12345);
    std::vector<fqsm::RawId> order;
    for (fqsm::RawId i = 1; i <= 50; ++i) order.push_back(i);
    std::shuffle(order.begin(), order.end(), random);
    for (std::size_t k = 0; k < 30; ++k) {
        EXPECT_TRUE(line.erase(order[k]));
        model.erase(order[k]);
    }
    EXPECT_FALSE(line.erase(order[0]));
    EXPECT_EQ(line.size(), std::size_t{20});
    for (const auto& [id, value] : model)
        EXPECT_EQ(as_int(line.find(id)), value);
    EXPECT_TRUE(tests::erased::collect(line) == model);
    EXPECT_EQ(tests::erased::steps(line), std::size_t{20});

    const int global = 99;
    line.set_global(&global);
    EXPECT_EQ(as_int(line.global()), 99);

    Line copy(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<int>());
    copy.clone(line);
    EXPECT_TRUE(tests::erased::collect(copy) == model);
    EXPECT_EQ(as_int(copy.global()), 99);

    line.clear();
    EXPECT_EQ(line.size(), std::size_t{0});
    EXPECT_TRUE(line.cursor_begin() == line.cursor_end());
    EXPECT_EQ(copy.size(), std::size_t{20});
}

void erased_line_global_absent()
{
    fqsm::erased::Line line(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<NoDefault>());
    EXPECT_TRUE(line.global() == nullptr);
    const NoDefault initial{5};
    line.set_global(&initial);
    EXPECT_EQ(static_cast<const NoDefault*>(line.global())->value, 5);
    const NoDefault next{6};
    line.set_global(&next);
    EXPECT_EQ(static_cast<const NoDefault*>(line.global())->value, 6);
}

void erased_line_lifetime()
{
    using erased::Counted;
    using erased::Counts;

    Counts counts;
    {
        fqsm::erased::Line line(fqsm::erased::ops_of<Counted>(), fqsm::erased::ops_of<int>());
        Counted a(counts, "a");
        Counted b(counts, "b");
        for (fqsm::RawId i = 0; i < 10; ++i)
            line.insert(i, &a);
        EXPECT_EQ(counts.alive(), 2 + 10);

        line.insert(3, &b);
        EXPECT_EQ(counts.alive(), 2 + 10);
        EXPECT_EQ(static_cast<const Counted*>(line.find(3))->text, std::string("b"));

        Counted c(counts, "c");
        line.emplace_move(4, &c);
        EXPECT_EQ(static_cast<const Counted*>(line.find(4))->text, std::string("c"));
        EXPECT_EQ(counts.alive(), 3 + 10);

        line.erase(0);
        line.erase(9);
        EXPECT_EQ(counts.alive(), 3 + 8);
        EXPECT_EQ(static_cast<const Counted*>(line.find(3))->text, std::string("b"));

        fqsm::erased::Line copy(line);
        EXPECT_EQ(counts.alive(), 3 + 16);
        line.clear();
        EXPECT_EQ(counts.alive(), 3 + 8);
    }
    EXPECT_EQ(counts.alive(), 0);
}

// Three nested overlays against sequential application of the same patches.
void erased_overlay_nested()
{
    std::mt19937 random(777);
    std::uniform_int_distribution<int> pick(1, 60);
    std::uniform_int_distribution<int> action(0, 2);

    fqsm::erased::Line root(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<int>());
    std::map<fqsm::RawId, int> model;
    for (int i = 1; i <= 40; ++i) {
        root.insert(static_cast<fqsm::RawId>(i), &i);
        model[static_cast<fqsm::RawId>(i)] = i;
    }

    fqsm::erased::PatchLine layers[3] = {erased::int_patch(), erased::int_patch(), erased::int_patch()};
    for (int layer = 0; layer < 3; ++layer) {
        for (int k = 0; k < 25; ++k) {
            const auto id = static_cast<fqsm::RawId>(pick(random));
            const int value = 1000 * (layer + 1) + k;
            erased::put(layers[layer], id, value, action(random) == 0);
        }
    }

    const fqsm::erased::FutureLine first(root, layers[0]);
    const fqsm::erased::FutureLine second(first, layers[1]);
    const fqsm::erased::FutureLine third(second, layers[2]);
    const fqsm::erased::ReadLine* views[] = {&first, &second, &third};

    for (int layer = 0; layer < 3; ++layer) {
        erased::apply_to(layers[layer], model);
        const auto& view = *views[layer];

        EXPECT_TRUE(erased::collect(view) == model);
        EXPECT_EQ(erased::steps(view), model.size());
        EXPECT_EQ(view.size(), model.size());
        for (fqsm::RawId id = 0; id <= 61; ++id) {
            const auto found = model.find(id);
            EXPECT_EQ(view.contains(id), found != model.end());
            if (found != model.end()) EXPECT_EQ(as_int(view.find(id)), found->second);
        }
    }
    EXPECT_EQ(root.size(), std::size_t{40});

    const int five = 5, six = 6;
    layers[1].set_global(&five);
    EXPECT_EQ(as_int(third.global()), 5);
    layers[2].set_global(&six);
    EXPECT_EQ(as_int(third.global()), 6);
    EXPECT_EQ(as_int(first.global()), 0);
}

void erased_delta_modes()
{
    using fqsm::erased::DeltaCursor;
    using fqsm::erased::DeltaLayer;
    using fqsm::erased::DeltaMode;

    fqsm::erased::Line state(fqsm::erased::ops_of<int>(), fqsm::erased::ops_of<int>());
    for (int i = 1; i <= 10; ++i)
        state.insert(static_cast<fqsm::RawId>(i), &i);

    auto patch = erased::int_patch();
    erased::put(patch, 1, 1, true);      // removed
    erased::put(patch, 2, 20);           // updated
    erased::put(patch, 3, 30);
    erased::put(patch, 3, 31, true);     // removed, last value kept
    erased::put(patch, 11, 110);         // added
    erased::put(patch, 12, 120, true);   // deletion of an absent id: only in all()

    const auto gather = [&](DeltaMode mode, DeltaLayer layer) {
        std::set<fqsm::RawId> out;
        for (auto it = DeltaCursor::begin(state, patch, mode, layer), end = DeltaCursor::end(state, patch, mode, layer); not (it == end); ++it)
            out.insert((*it).id);
        return out;
    };
    using Ids = std::set<fqsm::RawId>;

    for (const auto mode : {DeltaMode::clean, DeltaMode::dirty}) {
        EXPECT_TRUE(gather(mode, DeltaLayer::added) == Ids({11}));
        EXPECT_TRUE(gather(mode, DeltaLayer::updated) == Ids({2}));
        EXPECT_TRUE(gather(mode, DeltaLayer::removed) == Ids({1, 3}));
    }
    EXPECT_TRUE(gather(DeltaMode::clean, DeltaLayer::all) == Ids({1, 2, 3, 11, 12}));
    EXPECT_TRUE(gather(DeltaMode::clean, DeltaLayer::addedOrUpdated) == Ids({2, 11}));
    EXPECT_TRUE(gather(DeltaMode::dirty, DeltaLayer::all) == Ids({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}));
    EXPECT_TRUE(gather(DeltaMode::dirty, DeltaLayer::addedOrUpdated) == Ids({2, 4, 5, 6, 7, 8, 9, 10, 11}));

    for (auto it = DeltaCursor::begin(state, patch, DeltaMode::dirty, DeltaLayer::all), end = DeltaCursor::end(state, patch, DeltaMode::dirty, DeltaLayer::all); not (it == end); ++it) {
        const auto change = *it;
        if (change.id == 3) {
            EXPECT_EQ(as_int(change.before), 3);
            EXPECT_TRUE(change.after == nullptr);
        }
        if (change.id == 4) {
            EXPECT_TRUE(change.tainted);
            EXPECT_EQ(as_int(change.after), 4);
        }
    }

    EXPECT_FALSE(fqsm::erased::delta_empty(state, patch, DeltaMode::clean, DeltaLayer::added));
    const auto empty = erased::int_patch();
    EXPECT_TRUE(fqsm::erased::delta_empty(state, empty, DeltaMode::clean, DeltaLayer::all));
    EXPECT_FALSE(fqsm::erased::delta_empty(state, empty, DeltaMode::dirty, DeltaLayer::all));
    EXPECT_TRUE(fqsm::erased::delta_empty(state, empty, DeltaMode::dirty, DeltaLayer::updated));
}

} // namespace tests
