#include "_common.h"

#include <deque>
#include <map>
#include <string>

#include <base/serialization.h>
#include <fQSM/api/interface.h>

namespace local {
    using namespace fqsm::api;

    struct JobTable {
        std::map<int, std::string> entries;
    };

    struct MyFieldNeedsCodec : Entity<MyFieldNeedsCodec> {
        struct Quantum {
            JobTable jobs;
        };

        using Reactions = DefaultReactions;
    };

    struct GoodSerializable : Entity<GoodSerializable> {
        struct Quantum {
            std::map<int, std::string> temp_field;
        };

        using Reactions = DefaultReactions;
    };

    struct NeedsManualCodec : Entity<NeedsManualCodec> {
        struct Quantum {
            std::deque<std::string> jobs;
        };

        using Reactions = DefaultReactions;
    };
}

namespace base::serialization {

template<>
struct codec<local::JobTable, void> {
    static auto read(std::istream& in) -> local::JobTable {
        local::JobTable out{};
        detail::expect(in, '{');
        out.entries = detail::read<std::map<int, std::string>>(in);
        detail::expect(in, '}');
        return out;
    }

    static void write(std::ostream& out, const local::JobTable& value) {
        out << '{';
        detail::write(out, value.entries);
        out << '}';
    }
};


// IMPORTANT:
// this test demonstrates, that codec for "Good enough" Aspect is unnecessary
//template<>
//struct codec<local::GoodSerializable::Quantum, void> {
// ...
// ...
//};
// you dond need strings above

template<>
struct codec<local::NeedsManualCodec::Quantum, void> {
    static auto read(std::istream& in) -> local::NeedsManualCodec::Quantum {
        local::NeedsManualCodec::Quantum out{};
        detail::expect(in, '{');
        detail::read_sequence(in, '[', ']', [&] {
            out.jobs.push_back(detail::read<std::string>(in));
        });
        detail::expect(in, '}');
        return out;
    }

    static void write(std::ostream& out, const local::NeedsManualCodec::Quantum& value) {
        out << '{';
        detail::write_sequence(out, '[', ']', value.jobs, [&](const auto& entry) {
            detail::write(out, entry);
        });
        out << '}';
    }
};

} // namespace base::serialization

namespace tests {

void serialization()
{
    using namespace local;
    using namespace fqsm::api;

    const Schema schema = ask::schema::merge({
        ask::schema::aspect<MyFieldNeedsCodec>(),
        ask::schema::aspect<GoodSerializable>(),
        ask::schema::aspect<NeedsManualCodec>(),
    });

    context::Realm main(schema);

    ask::item::create<MyFieldNeedsCodec>(main, {.jobs = {.entries = {{1, "one"}, {2, "two"}}}});
    ask::item::create<GoodSerializable>(main, {.temp_field = {{7, "seven"}}});
    ask::item::create<NeedsManualCodec>(main, {.jobs = std::deque<std::string>{"nine", "ten"}});
}

} // namespace tests
