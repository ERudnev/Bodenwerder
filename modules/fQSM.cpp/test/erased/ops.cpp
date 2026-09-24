#include "_common.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

#include <fQSM/api/interface.h>
#include <fQSM/erased/descriptor.h>

#ifdef _MSC_VER
#pragma warning(disable: 4324)
#endif

namespace {
    struct Plain {
        int a;
        float b;
    };

    struct Rich {
        std::vector<int> numbers;
        std::string name;
        std::unordered_set<std::uint64_t> tags;
        bool operator==(const Rich&) const = default;
    };

    struct NoDefault {
        explicit NoDefault(int v) : value(v) {}
        int value;
        bool operator==(const NoDefault&) const = default;
    };

    struct alignas(32) Wide {
        double x, y, z, w;
    };

    template<typename T>
    struct Storage {
        alignas(T) unsigned char bytes[sizeof(T)];
        T* get() { return reinterpret_cast<T*>(bytes); }
    };

    namespace local {
        using namespace fqsm::api;

        struct Host : Entity<Host> {
            struct Quantum { integer value; };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };

        struct Part : Feature<Part, Host> {
            struct Quantum { integer value; };
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };

        struct Crew : Group<Crew, Host, Host> {
            struct Internals : DefaultInternals{};
            static const Behavior customAspectReactions() { return {}; }
        };
    }
}

namespace tests {

void erased_ops_trivial()
{
    const auto& ops = fqsm::erased::ops_of<Plain>();
    EXPECT_EQ(ops.size, sizeof(Plain));
    EXPECT_EQ(ops.align, alignof(Plain));
    EXPECT_TRUE(ops.construct != nullptr);
    EXPECT_TRUE(ops.equal == nullptr);
    EXPECT_TRUE(&ops == &fqsm::erased::ops_of<Plain>());

    Storage<Plain> a, b;
    ops.construct(a.get());
    EXPECT_EQ(a.get()->a, 0);

    const Plain source{7, 2.5f};
    ops.copy(b.get(), &source);
    EXPECT_EQ(b.get()->a, 7);
    ops.destroy(a.get());
    ops.copy(a.get(), b.get());
    EXPECT_EQ(a.get()->a, 7);
    ops.destroy(a.get());
    ops.destroy(b.get());
}

void erased_ops_rich()
{
    const auto& ops = fqsm::erased::ops_of<Rich>();
    EXPECT_TRUE(ops.construct != nullptr);
    EXPECT_TRUE(ops.equal != nullptr);

    const Rich source{{1, 2, 3}, "a fairly long name that does not fit a small buffer", {10, 20}};
    Storage<Rich> copied, moved;
    ops.copy(copied.get(), &source);
    EXPECT_TRUE(ops.equal(copied.get(), &source));
    EXPECT_EQ(copied.get()->numbers.size(), std::size_t{3});
    EXPECT_TRUE(copied.get()->tags.contains(20));

    ops.move(moved.get(), copied.get());
    EXPECT_EQ(moved.get()->name, source.name);
    EXPECT_TRUE(moved.get()->tags.contains(10));
    EXPECT_TRUE(copied.get()->numbers.empty());

    moved.get()->numbers.push_back(4);
    EXPECT_FALSE(ops.equal(moved.get(), &source));

    ops.destroy(copied.get());
    ops.destroy(moved.get());
}

void erased_ops_no_default()
{
    const auto& ops = fqsm::erased::ops_of<NoDefault>();
    EXPECT_TRUE(ops.construct == nullptr);
    EXPECT_TRUE(ops.equal != nullptr);

    const NoDefault source{42};
    Storage<NoDefault> copy;
    ops.copy(copy.get(), &source);
    EXPECT_EQ(copy.get()->value, 42);
    ops.destroy(copy.get());
}

void erased_ops_aligned()
{
    const auto& ops = fqsm::erased::ops_of<Wide>();
    EXPECT_EQ(ops.align, std::size_t{32});
    EXPECT_EQ(ops.size, std::size_t{32});
}

void erased_describe()
{
    using namespace local;
    using fqsm::erased::Category;

    const auto host = fqsm::erased::describe<Host>();
    EXPECT_TRUE(host.id == fqsm::TypeId<Host>);
    EXPECT_TRUE(host.category == Category::entity);
    EXPECT_FALSE(host.host.has_value());
    EXPECT_TRUE(host.quantum == &fqsm::erased::ops_of<Host::Quantum>());
    EXPECT_TRUE(host.assembleGlobal == nullptr);
    EXPECT_TRUE(host.groupErase == nullptr);

    const auto part = fqsm::erased::describe<Part>();
    EXPECT_TRUE(part.category == Category::feature);
    EXPECT_TRUE(part.host == fqsm::TypeId<Host>);
    EXPECT_FALSE(part.reactions.empty());

    const auto crew = fqsm::erased::describe<Crew>();
    EXPECT_TRUE(crew.category == Category::group);
    EXPECT_TRUE(crew.element == fqsm::TypeId<Host>);

    Crew::Quantum members;
    crew.groupInsert(&members, 5);
    crew.groupInsert(&members, 6);
    EXPECT_EQ(members.size(), std::size_t{2});
    EXPECT_TRUE(crew.groupErase(&members, 5));
    EXPECT_FALSE(crew.groupErase(&members, 5));
    EXPECT_EQ(members.size(), std::size_t{1});
}

} // namespace tests
