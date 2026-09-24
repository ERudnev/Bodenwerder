#include "_common.h"

#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include <fQSM/api/interface.h>
#include <fQSM/erased/future_line.h>
#include <fQSM/erased/line.h>
#include <fQSM/erased/patch_line.h>
#include <fQSM/view/workers.h>

namespace {
    namespace local {
        using namespace fqsm::api;

        struct Tree : Entity<Tree> {
            struct Quantum {
                integer height;
                string name;
            };
            struct Global {
                integer planted = 0;
            };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
    }
}

namespace tests {

void erased_items_view()
{
    using namespace local;
    using Id = fqsm::Id<Tree>;
    using View = fqsm::view::Slot<Tree>;
    const auto descriptor = fqsm::erased::describe<Tree>();

    fqsm::erased::Line line(descriptor);
    View reality(line, &line, nullptr);
    auto& items = reality.items();
    EXPECT_TRUE(items.empty());

    std::map<Id, integer> expected;
    for (int i = 1; i <= 20; ++i) {
        const Id id = Id::generate_random();
        items.insert(id, Tree::Quantum{i, "tree" + std::to_string(i)});
        expected.emplace(id, i);
    }
    EXPECT_EQ(items.size(), std::size_t{20});

    const Id first = expected.begin()->first;
    EXPECT_TRUE(items.contains(first));
    EXPECT_EQ(items.find(first)->height, expected.at(first));
    EXPECT_EQ(items.at(first).height, expected.at(first));
    EXPECT_TRUE(items.get(first).has_value());

    const Id missing = Id::generate_random();
    EXPECT_TRUE(items.find(missing) == nullptr);
    bool thrown = false;
    try { (void)items.at(missing); } catch (const std::out_of_range&) { thrown = true; }
    EXPECT_TRUE(thrown);

    const Tree::Quantum replacement{100, "replaced"};
    items.insert(first, replacement);
    expected[first] = 100;
    EXPECT_EQ(items.size(), std::size_t{20});
    EXPECT_EQ(items.find(first)->name, std::string("replaced"));

    for (auto [id, tree] : items)
        tree.height += 1;
    for (auto& [id, height] : expected)
        height += 1;

    std::map<Id, integer> seen;
    const auto& readOnly = std::as_const(reality).items();
    for (const auto entry : readOnly)
        seen.emplace(entry.id, entry.value.height);
    EXPECT_TRUE(seen == expected);

    auto it = readOnly.begin();
    EXPECT_TRUE(expected.contains(it->id));
    EXPECT_EQ(it->value.height, expected.at(it->id));

    EXPECT_TRUE(items.erase(first));
    EXPECT_FALSE(items.erase(first));
    EXPECT_EQ(items.size(), std::size_t{19});

    reality.global().planted = 3;
    fqsm::erased::Line cloned(descriptor);
    cloned.clone(line);
    const View copied(cloned, nullptr, nullptr);
    EXPECT_EQ(copied.items().size(), std::size_t{19});
    EXPECT_EQ(copied.global().planted, 3);
    EXPECT_FALSE(copied.items().contains(first));

    items.clear();
    items.reserve(8);
    EXPECT_TRUE(items.empty());
    EXPECT_EQ(copied.items().size(), std::size_t{19});
}

void erased_items_future()
{
    using namespace local;
    using Id = fqsm::Id<Tree>;
    using View = fqsm::view::Slot<Tree>;
    const auto descriptor = fqsm::erased::describe<Tree>();

    fqsm::erased::Line line(descriptor);
    View reality(line, &line, nullptr);
    const Id kept = Id::generate_random();
    const Id changed = Id::generate_random();
    const Id removed = Id::generate_random();
    reality.items().insert(kept, Tree::Quantum{1, "kept"});
    reality.items().insert(changed, Tree::Quantum{2, "changed"});
    reality.items().insert(removed, Tree::Quantum{3, "removed"});

    fqsm::erased::PatchLine patch(descriptor);
    fqsm::erased::FutureLine futureLine(line, patch);
    View future(futureLine, nullptr, &futureLine);
    const Id added = Id::generate_random();
    future.put_add(added, Tree::Quantum{4, "added"});
    future.get_modification_access(changed).height = 20;
    future.put_deletion(removed);
    future.get_access_global().planted = 7;

    const auto& view = std::as_const(future).items();
    EXPECT_EQ(view.size(), std::size_t{3});
    EXPECT_TRUE(view.contains(added));
    EXPECT_FALSE(view.contains(removed));
    EXPECT_EQ(view.find(changed)->height, 20);
    EXPECT_EQ(reality.items().find(changed)->height, 2);
    EXPECT_EQ(std::as_const(future).global().planted, 7);
    EXPECT_EQ(reality.global().planted, 0);

    std::set<Id> ids;
    for (const auto entry : view)
        ids.insert(entry.id);
    EXPECT_TRUE(ids == std::set<Id>({kept, changed, added}));

    bool thrown = false;
    try { future.items().insert(kept, Tree::Quantum{}); } catch (const std::logic_error&) { thrown = true; }
    EXPECT_TRUE(thrown);

    fqsm::erased::PatchLine nestedPatch(descriptor);
    fqsm::erased::FutureLine nestedLine(futureLine, nestedPatch);
    View nested(nestedLine, nullptr, &nestedLine);
    nested.put_deletion(added);
    nested.put_add(removed, Tree::Quantum{5, "again"});
    const auto& nestedView = std::as_const(nested).items();
    ids.clear();
    for (const auto entry : nestedView)
        ids.insert(entry.id);
    EXPECT_TRUE(ids == std::set<Id>({kept, changed, removed}));
    EXPECT_EQ(nestedView.size(), std::size_t{3});
    EXPECT_EQ(nestedView.find(changed)->height, 20);
}

} // namespace tests
