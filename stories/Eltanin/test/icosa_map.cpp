#include "geo/details/icosaMap.h"

#include <base/testing/macros.h>
#include <base/testing/runner.h>

#include <cstddef>
#include <cstdint>

namespace tests {

    void stitch_preserves_packed_facies() {
        const eltanin::geo::IcosaPack pack{.edgeBase = 1, .tessellation = 0};
        pack.cacheWeld();
        eltanin::geo::IcosaMap<std::uint16_t, eltanin::geo::StitchMode::PickFirst> covers{pack, 0};
        constexpr std::uint16_t first = 0x0002;
        constexpr std::uint16_t second = 0x0100;
        for (int index = 0; index < pack.storedCount(); ++index) {
            const auto slot = pack.slotOf(index);
            covers.at(slot) = slot.diamond % 2 == 0 ? first : second;
        }

        std::size_t mixedGroups = 0;
        for (const auto& group : pack.weld->groups) {
            for (const auto slot : group.slots) {
                if (covers.at(slot) != covers.at(group.slots.front())) {
                    ++mixedGroups;
                    break;
                }
            }
        }
        EXPECT_TRUE(mixedGroups > 0);

        covers.stitch();
        for (const auto& group : pack.weld->groups) {
            const auto stitched = covers.at(group.slots.front());
            EXPECT_TRUE(stitched == (group.slots.front().diamond % 2 == 0 ? first : second));
            for (const auto slot : group.slots)
                EXPECT_TRUE(covers.at(slot) == stitched);
        }
    }

}

#define ICOSA_MAP_TESTS(X) X(stitch_preserves_packed_facies)

BASETEST_FORWARD_DECLARE_TESTS(ICOSA_MAP_TESTS)

int main() {
    const auto summary = base::testing::run_tests(BASETEST_MAKE_LIST_TESTS(ICOSA_MAP_TESTS));
    return summary.ok() ? 0 : 1;
}
