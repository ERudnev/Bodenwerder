#include "physics/horn.h"

#include <base/logging.h>
#include <base/testing/macros.h>
#include <base/testing/runner.h>

#include <cmath>
#include <format>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

    using namespace base::common_types;
    using eltanin::phys::horn::orientation;

    constexpr float k_err_eps = 1.0e-8f;

    // Same order as assets/Eltanin/atomic/kube4m.atomic (game meters, diameter 4 → ±2).
    auto kube_rest() -> vector<vec3> {
        return vector<vec3>{
            {-2.0f, -2.0f, -2.0f},
            {2.0f, -2.0f, -2.0f},
            {2.0f, 2.0f, -2.0f},
            {-2.0f, 2.0f, -2.0f},
            {-2.0f, -2.0f, 2.0f},
            {2.0f, -2.0f, 2.0f},
            {2.0f, 2.0f, 2.0f},
            {-2.0f, 2.0f, 2.0f},
        };
    }

    auto uniform_masses(std::size_t n, float mass = 1.0f) -> vector<float> {
        return vector<float>(n, mass);
    }

    auto apply_rotation(const quat& q, const vector<vec3>& rest) -> vector<vec3> {
        vector<vec3> world;
        world.reserve(rest.size());
        for (const vec3& p : rest) {
            world.push_back(q * p);
        }
        return world;
    }

    auto center(const vector<vec3>& points, const vector<float>& masses) -> vector<vec3> {
        vec3 com{0.0f, 0.0f, 0.0f};
        float mass_sum = 0.0f;
        for (std::size_t i = 0; i < points.size(); ++i) {
            com += points[i] * masses[i];
            mass_sum += masses[i];
        }
        com /= mass_sum;
        vector<vec3> out;
        out.reserve(points.size());
        for (const vec3& p : points) {
            out.push_back(p - com);
        }
        return out;
    }

    auto weighted_msq_error(const quat& q, const vector<vec3>& rest, const vector<vec3>& world, const vector<float>& masses) -> float {
        float err = 0.0f;
        float mass_sum = 0.0f;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            const vec3 delta = (q * rest[i]) - world[i];
            err += masses[i] * glm::dot(delta, delta);
            mass_sum += masses[i];
        }
        return err / mass_sum;
    }

    void expect_fits(const quat& known, const vector<vec3>& rest, const vector<float>& masses) {
        const vector<vec3> world = apply_rotation(known, rest);
        const quat got = orientation(rest, world, masses);
        const float err = weighted_msq_error(got, rest, world, masses);
        EXPECT_TRUE(err < k_err_eps) << "msq error " << err;
    }

    struct Lcg {
        std::uint32_t state;
        auto next_u32() -> std::uint32_t {
            state = state * 1664525u + 1013904223u;
            return state;
        }
        auto next_unit() -> float {
            return static_cast<float>(next_u32() >> 8) * (1.0f / 16777216.0f);
        }
        auto next_signed() -> float {
            return next_unit() * 2.0f - 1.0f;
        }
    };

    auto random_unit_quat(Lcg& rng) -> quat {
        const float u1 = rng.next_unit();
        const float u2 = rng.next_unit();
        const float u3 = rng.next_unit();
        const float sq1 = std::sqrt(1.0f - u1);
        const float squ = std::sqrt(u1);
        constexpr float tau = 2.0f * std::numbers::pi_v<float>;
        return glm::normalize(quat(
            squ * std::cos(tau * u3),
            sq1 * std::sin(tau * u2),
            sq1 * std::cos(tau * u2),
            squ * std::sin(tau * u3)));
    }

    auto random_axis(Lcg& rng) -> vec3 {
        vec3 a{rng.next_signed(), rng.next_signed(), rng.next_signed()};
        const float len = glm::length(a);
        if (len < 1.0e-8f) {
            return vec3{0.0f, 1.0f, 0.0f};
        }
        return a / len;
    }

    struct SweepFail {
        int index;
        float err;
        quat known;
        quat got;
    };

    auto run_sweep(const vector<vec3>& rest, const vector<float>& masses, int random_count, int smooth_steps) -> base::maybe<SweepFail> {
        Lcg rng{.state = 0xC0FFEEu};

        for (int i = 0; i < random_count; ++i) {
            const quat known = random_unit_quat(rng);
            const vector<vec3> world = apply_rotation(known, rest);
            const quat got = orientation(rest, world, masses);
            const float err = weighted_msq_error(got, rest, world, masses);
            if (err >= k_err_eps) {
                return SweepFail{.index = i, .err = err, .known = known, .got = got};
            }
        }

        quat q = quat(1.0f, 0.0f, 0.0f, 0.0f);
        for (int i = 0; i < smooth_steps; ++i) {
            const float angle = 0.017453292f;
            const quat step = glm::angleAxis(angle, random_axis(rng));
            q = glm::normalize(step * q);
            const vector<vec3> world = apply_rotation(q, rest);
            const quat got = orientation(rest, world, masses);
            const float err = weighted_msq_error(got, rest, world, masses);
            if (err >= k_err_eps) {
                return SweepFail{.index = random_count + i, .err = err, .known = q, .got = got};
            }
        }
        return {};
    }

    void report_fail(const char* label, const SweepFail& fail) {
        ADD_FAILURE(std::format(
            "{} first fail @{} msq={} known=({}, {}, {}, {}) got=({}, {}, {}, {}) |dot|={}",
            label,
            fail.index,
            fail.err,
            fail.known.w, fail.known.x, fail.known.y, fail.known.z,
            fail.got.w, fail.got.x, fail.got.y, fail.got.z,
            std::abs(glm::dot(fail.known, fail.got))));
    }

} // namespace

namespace tests {

    void horn_identity() {
        const auto rest = kube_rest();
        expect_fits(quat(1.0f, 0.0f, 0.0f, 0.0f), rest, uniform_masses(rest.size()));
    }

    void horn_rot90_x() {
        const auto rest = kube_rest();
        const quat known = glm::angleAxis(std::numbers::pi_v<float> * 0.5f, vec3{1.0f, 0.0f, 0.0f});
        expect_fits(known, rest, uniform_masses(rest.size()));
    }

    void horn_rot90_y() {
        const auto rest = kube_rest();
        const quat known = glm::angleAxis(std::numbers::pi_v<float> * 0.5f, vec3{0.0f, 1.0f, 0.0f});
        expect_fits(known, rest, uniform_masses(rest.size()));
    }

    void horn_rot90_z() {
        const auto rest = kube_rest();
        const quat known = glm::angleAxis(std::numbers::pi_v<float> * 0.5f, vec3{0.0f, 0.0f, 1.0f});
        expect_fits(known, rest, uniform_masses(rest.size()));
    }

    void horn_arbitrary_quat() {
        const auto rest = kube_rest();
        const quat known = glm::normalize(quat(0.3f, 0.2f, 0.5f, 0.8f));
        expect_fits(known, rest, uniform_masses(rest.size()));
    }

    void horn_translation_invariant() {
        const auto rest = kube_rest();
        const quat known = glm::normalize(quat(0.3f, 0.2f, 0.5f, 0.8f));
        const auto masses = uniform_masses(rest.size());
        vector<vec3> world = apply_rotation(known, rest);
        const vec3 shift{3.5f, -2.0f, 7.25f};
        for (vec3& p : world) {
            p += shift;
        }
        const auto rest_c = center(rest, masses);
        const auto world_c = center(world, masses);
        const quat got = orientation(rest_c, world_c, masses);
        const float err = weighted_msq_error(got, rest_c, world_c, masses);
        EXPECT_TRUE(err < k_err_eps) << "msq error " << err;
    }

    void horn_weighted_masses() {
        const auto rest = kube_rest();
        const quat known = glm::normalize(quat(0.1f, -0.4f, 0.6f, 0.7f));
        vector<float> masses = uniform_masses(rest.size());
        masses[0] = 0.25f;
        masses[1] = 4.0f;
        masses[2] = 1.5f;
        masses[3] = 0.5f;
        masses[4] = 8.0f;
        masses[5] = 2.0f;
        masses[6] = 0.75f;
        masses[7] = 3.0f;
        expect_fits(known, rest, masses);
    }

    void horn_sweep_random_and_smooth() {
        const auto rest = kube_rest();
        const auto masses = uniform_masses(rest.size());
        constexpr int k_random = 5000;
        constexpr int k_smooth = 5000;
        base::message(std::format("horn sweep: {} random + {} smooth (~1°/step) on kube4m rest", k_random, k_smooth));
        if (const auto fail = run_sweep(rest, masses, k_random, k_smooth)) {
            report_fail("horn_sweep", *fail);
        }
    }

    void horn_sweep_nested_grid() {
        const auto rest = kube_rest();
        const auto masses = uniform_masses(rest.size());
        constexpr int k_axis = 12;
        constexpr int k_angles = 36;
        int index = 0;
        for (int ia = 0; ia < k_axis; ++ia) {
            for (int ib = 0; ib < k_axis; ++ib) {
                const float theta = std::numbers::pi_v<float> * static_cast<float>(ia) / static_cast<float>(k_axis);
                const float phi = 2.0f * std::numbers::pi_v<float> * static_cast<float>(ib) / static_cast<float>(k_axis);
                const vec3 axis{
                    std::sin(theta) * std::cos(phi),
                    std::sin(theta) * std::sin(phi),
                    std::cos(theta),
                };
                const float axis_len = glm::length(axis);
                if (axis_len < 1.0e-8f) {
                    continue;
                }
                const vec3 n = axis / axis_len;
                for (int ic = 0; ic < k_angles; ++ic) {
                    const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(ic) / static_cast<float>(k_angles);
                    const quat known = glm::angleAxis(angle, n);
                    const vector<vec3> world = apply_rotation(known, rest);
                    const quat got = orientation(rest, world, masses);
                    const float err = weighted_msq_error(got, rest, world, masses);
                    if (err >= k_err_eps) {
                        report_fail("horn_grid", SweepFail{.index = index, .err = err, .known = known, .got = got});
                        return;
                    }
                    ++index;
                }
            }
        }
        base::message(std::format("horn_grid: OK {} orientations", index));
    }

}

#define HORN_TESTS(X) \
    X(horn_identity) \
    X(horn_rot90_x) \
    X(horn_rot90_y) \
    X(horn_rot90_z) \
    X(horn_arbitrary_quat) \
    X(horn_translation_invariant) \
    X(horn_weighted_masses) \
    X(horn_sweep_random_and_smooth) \
    X(horn_sweep_nested_grid) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(HORN_TESTS)

int main() {
    const auto summary = base::testing::run_tests(BASETEST_MAKE_LIST_TESTS(HORN_TESTS));
    return summary.ok() ? 0 : 1;
}
