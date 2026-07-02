#include "_common.h"

#include <base/serialization.h>
#include <base/types/common_types.h>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {

    struct Job {
        int id;
        std::string name;
    };

    struct Snapshot {
        std::map<int, std::string> jobs;
        std::vector<int> workers;
        std::optional<Job> highlighted;
        base::common_types::timepoint created_at;
    };

}

namespace tests {

void serialization_roundtrip()
{
    const Snapshot source{
        .jobs = {
            {1, "first"},
            {2, "second job"},
        },
        .workers = {3, 5, 8},
        .highlighted = Job{7, "boss"},
        .created_at = base::common_types::timepoint{std::chrono::system_clock::duration{1234567}},
    };

    const auto encoded = base::serialization::to_string(source);
    const auto restored = base::serialization::from_string<Snapshot>(encoded);

    EXPECT_EQ(base::serialization::to_string(restored), encoded);
    EXPECT_EQ(restored.jobs.at(2), "second job");
    EXPECT_EQ(restored.workers.size(), std::size_t{3});
    EXPECT_TRUE(restored.highlighted.has_value());
    EXPECT_EQ(restored.highlighted->id, 7);
    EXPECT_EQ(restored.highlighted->name, "boss");
    EXPECT_EQ(restored.created_at, source.created_at);
}

} // namespace tests
