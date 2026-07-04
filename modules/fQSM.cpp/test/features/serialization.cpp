#include "_common.h"

#include <deque>
#include <map>
#include <string>

#include <base/serialization.h>
#include <fQSM/api/interface.h>

namespace {
namespace local {
    using namespace fqsm::api;

    // this type is like "needed by several aspects" and made outside of any Metaclass
    struct JobTable {
        std::map<int, std::string> entries;
    };

    // this Aspect works well with built-in (PFR-wrapped) serialization
    struct BoxSerializable : Entity<BoxSerializable> {
        struct Quantum {
            std::map<int, std::string> temp_field;
        };

        using Reactions = DefaultReactions;
    };

    // Quantum of this aspect has mix of fields, one requires custom serialization, other - works by default
    struct CustomFieldCodec : Entity<CustomFieldCodec> {
        struct Quantum {
            JobTable jobs;
            std::string goodField;
        };

        using Reactions = DefaultReactions;
    };

    // this Aspect has custom serializaion for entire Quantum type (must not rely on by-field serialization)
    struct CustomQuantumCodec : Entity<CustomQuantumCodec> {
        struct Quantum {
            std::deque<std::string> jobs;
        };

        using Reactions = DefaultReactions;
    };
}
} // namespace

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
//struct codec<local::BoxSerializable::Quantum, void> {
// ...
// ...
//};
// you dond need strings above

template<>
struct codec<local::CustomQuantumCodec::Quantum, void> {
    static auto read(std::istream& in) -> local::CustomQuantumCodec::Quantum {
        local::CustomQuantumCodec::Quantum out{};
        detail::expect(in, '{');
        detail::read_sequence(in, '[', ']', [&] {
            out.jobs.push_back(detail::read<std::string>(in));
        });
        detail::expect(in, '}');
        return out;
    }

    static void write(std::ostream& out, const local::CustomQuantumCodec::Quantum& value) {
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
        ask::schema::aspect<BoxSerializable>(),
        ask::schema::aspect<CustomFieldCodec>(),
        ask::schema::aspect<CustomQuantumCodec>(),
    });

    context::Realm main(schema);

    with<BoxSerializable>::create(main, {.temp_field = {{7, "seven"}}});
    with<CustomFieldCodec>::create(main, {.jobs = {.entries = {{1, "one"}, {2, "two"}}}, .goodField = "some text"});
    with<CustomQuantumCodec>::create(main, {.jobs = std::deque<std::string>{"nine", "ten"}});
}

} // namespace tests
