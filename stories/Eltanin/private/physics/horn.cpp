#include "physics/horn.h"

#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace eltanin::phys::horn {

    namespace {

        constexpr float k_jacobi_eps = 1.0e-12f;
        constexpr int k_jacobi_max_sweeps = 50;

        // Cyclic Jacobi for symmetric 4×4 (Numerical Recipes style).
        // On exit: d[i] = eigenvalues; columns of v = eigenvectors.
        // Does NOT treat a[][] as authoritative after return — use d[] and v[][].
        void jacobi_symmetric_4x4(float a[4][4], float d[4], float v[4][4]) {
            float b[4];
            float z[4];
            for (int ip = 0; ip < 4; ++ip) {
                for (int iq = 0; iq < 4; ++iq) {
                    v[ip][iq] = (ip == iq) ? 1.0f : 0.0f;
                }
                b[ip] = d[ip] = a[ip][ip];
                z[ip] = 0.0f;
            }

            for (int sweep = 0; sweep < k_jacobi_max_sweeps; ++sweep) {
                float sm = 0.0f;
                for (int ip = 0; ip < 3; ++ip) {
                    for (int iq = ip + 1; iq < 4; ++iq) {
                        sm += std::abs(a[ip][iq]);
                    }
                }
                if (sm <= k_jacobi_eps) {
                    return;
                }

                const float tresh = (sweep < 4) ? (0.2f * sm / 16.0f) : 0.0f;

                for (int ip = 0; ip < 3; ++ip) {
                    for (int iq = ip + 1; iq < 4; ++iq) {
                        float g = 100.0f * std::abs(a[ip][iq]);
                        if (sweep > 4 and (std::abs(d[ip]) + g) == std::abs(d[ip]) and (std::abs(d[iq]) + g) == std::abs(d[iq])) {
                            a[ip][iq] = 0.0f;
                            continue;
                        }
                        if (std::abs(a[ip][iq]) <= tresh) {
                            continue;
                        }

                        float h = d[iq] - d[ip];
                        float t;
                        if ((std::abs(h) + g) == std::abs(h)) {
                            t = a[ip][iq] / h;
                        } else {
                            // θ = cot(2φ) = (d_q − d_p) / (2 a_pq)
                            const float theta = 0.5f * h / a[ip][iq];
                            t = 1.0f / (std::abs(theta) + std::sqrt(1.0f + theta * theta));
                            if (theta < 0.0f) {
                                t = -t;
                            }
                        }
                        const float c = 1.0f / std::sqrt(1.0f + t * t);
                        const float s = t * c;
                        const float tau = s / (1.0f + c);
                        h = t * a[ip][iq];
                        z[ip] -= h;
                        z[iq] += h;
                        d[ip] -= h;
                        d[iq] += h;
                        a[ip][iq] = 0.0f;

                        auto rotate = [&](float& g_ref, float& h_ref) {
                            const float gv = g_ref;
                            const float hv = h_ref;
                            g_ref = gv - s * (hv + gv * tau);
                            h_ref = hv + s * (gv - hv * tau);
                        };

                        for (int j = 0; j < ip; ++j) {
                            rotate(a[j][ip], a[j][iq]);
                        }
                        for (int j = ip + 1; j < iq; ++j) {
                            rotate(a[ip][j], a[j][iq]);
                        }
                        for (int j = iq + 1; j < 4; ++j) {
                            rotate(a[ip][j], a[iq][j]);
                        }
                        for (int j = 0; j < 4; ++j) {
                            rotate(v[j][ip], v[j][iq]);
                        }
                    }
                }

                for (int ip = 0; ip < 4; ++ip) {
                    b[ip] += z[ip];
                    d[ip] = b[ip];
                    z[ip] = 0.0f;
                }
            }
        }

        // Horn 1987 N from M = Σ left·rightᵀ (Sxx = Σ x_l x_r, …). R satisfies right ≈ R·left.
        // Quaternion basis (w, x, y, z) = (q0, qx, qy, qz).
        void build_horn_N(const float M[3][3], float N[4][4]) {
            const float xx = M[0][0];
            const float xy = M[0][1];
            const float xz = M[0][2];
            const float yx = M[1][0];
            const float yy = M[1][1];
            const float yz = M[1][2];
            const float zx = M[2][0];
            const float zy = M[2][1];
            const float zz = M[2][2];

            N[0][0] = xx + yy + zz;
            N[0][1] = yz - zy;
            N[0][2] = zx - xz;
            N[0][3] = xy - yx;

            N[1][0] = N[0][1];
            N[1][1] = xx - yy - zz;
            N[1][2] = xy + yx;
            N[1][3] = zx + xz;

            N[2][0] = N[0][2];
            N[2][1] = N[1][2];
            N[2][2] = -xx + yy - zz;
            N[2][3] = yz + zy;

            N[3][0] = N[0][3];
            N[3][1] = N[1][3];
            N[3][2] = N[2][3];
            N[3][3] = -xx - yy + zz;
        }

        auto rayleigh(const float N[4][4], const float v[4]) -> float {
            float Nv[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (int i = 0; i < 4; ++i) {
                for (int k = 0; k < 4; ++k) {
                    Nv[i] += N[i][k] * v[k];
                }
            }
            float ray = 0.0f;
            for (int i = 0; i < 4; ++i) {
                ray += v[i] * Nv[i];
            }
            return ray;
        }

    } // namespace

    auto orientation(const vector<vec3>& rest_centered, const vector<vec3>& world_centered, const vector<float>& masses) -> quat {
        const std::size_t n = rest_centered.size();
        if (n == 0 or world_centered.size() != n or masses.size() != n) {
            return quat(1.0f, 0.0f, 0.0f, 0.0f);
        }

        // Horn: right ≈ R·left. M = Σ left·rightᵀ. Here left=rest, right=world ⇒ R: rest→world.
        float M[3][3] = {
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
        };
        for (std::size_t i = 0; i < n; ++i) {
            const float m = masses[i];
            const vec3& w = world_centered[i];
            const vec3& r = rest_centered[i];
            M[0][0] += m * r.x * w.x;
            M[0][1] += m * r.x * w.y;
            M[0][2] += m * r.x * w.z;
            M[1][0] += m * r.y * w.x;
            M[1][1] += m * r.y * w.y;
            M[1][2] += m * r.y * w.z;
            M[2][0] += m * r.z * w.x;
            M[2][1] += m * r.z * w.y;
            M[2][2] += m * r.z * w.z;
        }

        float N[4][4];
        build_horn_N(M, N);

        float N_work[4][4];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                N_work[i][j] = N[i][j];
            }
        }

        float evals[4];
        float V[4][4];
        jacobi_symmetric_4x4(N_work, evals, V);

        // Pick eigenvector that maximizes qᵀ N q (Horn objective), scored on the original N.
        int best = 0;
        float best_ray = -1.0e30f;
        for (int j = 0; j < 4; ++j) {
            const float col[4] = {V[0][j], V[1][j], V[2][j], V[3][j]};
            const float ray = rayleigh(N, col);
            if (ray > best_ray) {
                best_ray = ray;
                best = j;
            }
        }

        // Horn (w,x,y,z). GLM ctor is quat(w,x,y,z).
        const quat q(V[0][best], V[1][best], V[2][best], V[3][best]);
        return glm::normalize(q);
    }

}
