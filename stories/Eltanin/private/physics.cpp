#include "physics.h"

#include <cmath>

#include <glm/geometric.hpp>

#include <imgui.h>

namespace eltanin::phys {

    namespace {

        constexpr int64 k_fixed_step_us = 10'000; // 10 ms
        constexpr float k_fixed_dt_s = 0.01f;

        // Softened magic point-mass at origin.
        constexpr float k_gravity_soften = 0.25f;
        constexpr float k_soften2 = k_gravity_soften * k_gravity_soften;

    } // namespace

    void System::drawUi() {
        ImGui::TextUnformatted("Time scale");
        constexpr float k_scales[] = {
            1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f,
            1.0f,
            2.0f, 4.0f, 8.0f, 16.0f,
        };
        constexpr const char* k_labels[] = {
            "1/16", "1/8", "1/4", "1/2",
            "1",
            "x2", "x4", "x8", "x16",
        };
        for (int i = 0; i < 9; ++i) {
            if (i > 0) {
                ImGui::SameLine();
            }
            if (ImGui::RadioButton(k_labels[i], state.time_scale == k_scales[i])) {
                state.time_scale = k_scales[i];
            }
        }
    }

    void System::applyForces(fqsm::Direct<Atom>& atoms) {
        accelerations.resize(atoms.items.size());
        std::size_t slot = 0;
        for (auto [_, atom] : atoms.items) {
            // a = −μ r / r³ (softened), independent of particle mass.
            const float r2 = std::max(glm::dot(atom.current, atom.current), k_soften2);
            const float inv_r3 = 1.0f / (r2 * std::sqrt(r2));
            accelerations[slot++] = -k_central_mu * atom.current * inv_r3;
        }
    }

    void System::integrate(fqsm::Direct<Atom>& atoms) {
        const float dt2 = k_fixed_dt_s * k_fixed_dt_s;
        std::size_t slot = 0;
        for (auto [_, atom] : atoms.items) {
            const vec3 previous = atom.current;
            atom.current += atom.current - atom.prev + accelerations[slot++] * dt2;
            atom.prev = previous;
        }
    }

    void System::tick(Stewarding context) {
        // Jakobsen TimeStep: AccumulateForces → Verlet → SatisfyConstraints (later).
        fqsm::Direct<Atom> atoms = context;
        applyForces(atoms);
        integrate(atoms);
    }

    void System::step(Stewarding context, int64 dt_us) {
        debt_us += static_cast<int64>(static_cast<double>(dt_us) * static_cast<double>(state.time_scale));
        while (debt_us >= k_fixed_step_us) {
            tick(context);
            debt_us -= k_fixed_step_us;
        }
    }

    auto System::addParticle(Writing context, vec3 pos, vec3 velocity, float mass) -> Atom::Id {
        // Verlet: v ≈ (current − prev) / dt  ⇒  prev = current − v·dt
        return with<Atom>::create(context, Atom::Quantum{
            .current = pos,
            .prev = pos - velocity * k_fixed_dt_s,
            .mass = mass,
        });
    }

}
