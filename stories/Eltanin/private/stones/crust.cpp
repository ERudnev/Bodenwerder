#include "stones/crust.h"

#include <eltanin/geo/minerals.q1.h>

#include <cmath>
#include <cstdint>

namespace eltanin::geo {

    using namespace fqsm::api;

    namespace {

        constexpr int mixChannels = 16;
        constexpr int cubeEdge = 64;

        auto hash31(int x, int y, int z, int seed) -> float {
            auto mix = [](std::uint32_t value) -> std::uint32_t {
                value ^= value >> 16;
                value *= 0x7feb352du;
                value ^= value >> 15;
                value *= 0x846ca68bu;
                value ^= value >> 16;
                return value;
            };
            const std::uint32_t h = mix(std::uint32_t(x) * 73856093u ^ std::uint32_t(y) * 19349663u ^ std::uint32_t(z) * 83492791u ^ std::uint32_t(seed) * 2654435761u);
            return float(h >> 8) * (1.0f / 16777215.0f);
        }

        auto valueNoise(float x, float y, float z, int seed) -> float {
            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            const int z0 = static_cast<int>(std::floor(z));
            const float tx = x - static_cast<float>(x0);
            const float ty = y - static_cast<float>(y0);
            const float tz = z - static_cast<float>(z0);
            const float sx = tx * tx * (3.0f - 2.0f * tx);
            const float sy = ty * ty * (3.0f - 2.0f * ty);
            const float sz = tz * tz * (3.0f - 2.0f * tz);
            const float c000 = hash31(x0, y0, z0, seed);
            const float c100 = hash31(x0 + 1, y0, z0, seed);
            const float c010 = hash31(x0, y0 + 1, z0, seed);
            const float c110 = hash31(x0 + 1, y0 + 1, z0, seed);
            const float c001 = hash31(x0, y0, z0 + 1, seed);
            const float c101 = hash31(x0 + 1, y0, z0 + 1, seed);
            const float c011 = hash31(x0, y0 + 1, z0 + 1, seed);
            const float c111 = hash31(x0 + 1, y0 + 1, z0 + 1, seed);
            const float c00 = c000 + (c100 - c000) * sx;
            const float c10 = c010 + (c110 - c010) * sx;
            const float c01 = c001 + (c101 - c001) * sx;
            const float c11 = c011 + (c111 - c011) * sx;
            const float c0 = c00 + (c10 - c00) * sy;
            const float c1 = c01 + (c11 - c01) * sy;
            return c0 + (c1 - c0) * sz;
        }

        auto fbm(float x, float y, float z, int seed) -> float {
            float sum = 0.0f;
            float amp = 0.5f;
            float freq = 1.0f;
            for (int octave = 0; octave < 4; ++octave) {
                sum += amp * valueNoise(x * freq, y * freq, z * freq, seed + octave * 17);
                amp *= 0.5f;
                freq *= 2.0f;
            }
            return sum;
        }

        auto toByte(float value) -> unsigned char {
            const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
            return static_cast<unsigned char>(clamped * 255.0f + 0.5f);
        }

    } // namespace

    auto generateCrust() -> rmmr::resource::texture3array::CpuPresentation {
        const auto& table = Mineral::table();
        rmmr::resource::texture3array::CpuPresentation cpu{
            .layerSize = index3{cubeEdge, cubeEdge, cubeEdge},
            .layers = {},
        };
        cpu.layers.resize(static_cast<std::size_t>(mixChannels));
        const std::size_t voxelCount = static_cast<std::size_t>(cubeEdge * cubeEdge * cubeEdge);
        for (int channel = 0; channel < mixChannels; ++channel) {
            const vec3 albedo = channel < static_cast<int>(table.size()) ? table[static_cast<std::size_t>(channel)].albedo : vec3{1.0f, 1.0f, 1.0f};
            const float roughness = channel < static_cast<int>(table.size()) ? table[static_cast<std::size_t>(channel)].roughness : 0.5f;
            auto& layer = cpu.layers[static_cast<std::size_t>(channel)];
            layer.resize(voxelCount * 4u);
            const float freq = 3.0f + static_cast<float>(channel) * 0.37f;
            for (int z = 0; z < cubeEdge; ++z) {
                for (int y = 0; y < cubeEdge; ++y) {
                    for (int x = 0; x < cubeEdge; ++x) {
                        const float px = static_cast<float>(x) / static_cast<float>(cubeEdge) * freq;
                        const float py = static_cast<float>(y) / static_cast<float>(cubeEdge) * freq;
                        const float pz = static_cast<float>(z) / static_cast<float>(cubeEdge) * freq;
                        const float grain = fbm(px, py, pz, channel * 131);
                        const float vein = fbm(px * 2.3f, py * 0.4f, pz * 2.3f, 900 + channel);
                        const float mix = 0.62f + 0.28f * grain + 0.10f * (vein - 0.5f) * roughness;
                        const std::size_t index = static_cast<std::size_t>(x + cubeEdge * (y + cubeEdge * z)) * 4u;
                        layer[index] = toByte(albedo.x * mix);
                        layer[index + 1] = toByte(albedo.y * mix);
                        layer[index + 2] = toByte(albedo.z * mix);
                        layer[index + 3] = 255;
                    }
                }
            }
        }
        return cpu;
    }

}
