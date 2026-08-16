#include <eltanin/geo/minerals.q1.h>

#include <base/types/common_types.h>

namespace eltanin::geo {

using base::common_types::rgb;

const vector<Mineral>& Mineral::table() {
    static const vector<Mineral> table{
        Mineral{.name = "Ice", .density = 0.92f, .scale = 0.08f, .albedo = rgb(217, 235, 242), .roughness = 0.25f, .metalness = 0.0f, .hardness = 1.5f},
        Mineral{.name = "Olivine", .density = 3.32f, .scale = 0.25f, .albedo = rgb(64, 89, 38), .roughness = 0.72f, .metalness = 0.0f, .hardness = 6.5f},
        Mineral{.name = "Pyroxene", .density = 3.28f, .scale = 0.28f, .albedo = rgb(56, 46, 38), .roughness = 0.75f, .metalness = 0.0f, .hardness = 6.0f},
        Mineral{.name = "Feldspar", .density = 2.62f, .scale = 0.22f, .albedo = rgb(191, 184, 173), .roughness = 0.68f, .metalness = 0.0f, .hardness = 6.0f},
        Mineral{.name = "Clay", .density = 2.20f, .scale = 0.45f, .albedo = rgb(140, 107, 71), .roughness = 0.88f, .metalness = 0.0f, .hardness = 2.0f},
        Mineral{.name = "Carbonaceous", .density = 1.80f, .scale = 0.35f, .albedo = rgb(20, 18, 15), .roughness = 0.92f, .metalness = 0.0f, .hardness = 2.5f},
        Mineral{.name = "Iron", .density = 7.87f, .scale = 0.55f, .albedo = rgb(140, 133, 128), .roughness = 0.32f, .metalness = 1.0f, .hardness = 4.0f},
        Mineral{.name = "Nickel", .density = 8.91f, .scale = 0.55f, .albedo = rgb(158, 153, 140), .roughness = 0.30f, .metalness = 1.0f, .hardness = 4.0f},
        Mineral{.name = "Sulfides", .density = 4.61f, .scale = 0.40f, .albedo = rgb(115, 89, 38), .roughness = 0.48f, .metalness = 0.55f, .hardness = 4.0f},
        Mineral{.name = "Oxides", .density = 5.17f, .scale = 0.38f, .albedo = rgb(89, 46, 31), .roughness = 0.62f, .metalness = 0.20f, .hardness = 6.0f},
        Mineral{.name = "BaseMetals", .density = 8.40f, .scale = 0.50f, .albedo = rgb(184, 115, 56), .roughness = 0.38f, .metalness = 1.0f, .hardness = 3.0f},
        Mineral{.name = "PGM", .density = 21.0f, .scale = 0.70f, .albedo = rgb(140, 143, 148), .roughness = 0.22f, .metalness = 1.0f, .hardness = 4.5f},
        Mineral{.name = "RareEarths", .density = 7.00f, .scale = 0.42f, .albedo = rgb(102, 115, 97), .roughness = 0.55f, .metalness = 0.35f, .hardness = 5.0f},
        Mineral{.name = "Actinides", .density = 15.0f, .scale = 0.48f, .albedo = rgb(71, 77, 56), .roughness = 0.45f, .metalness = 0.70f, .hardness = 6.0f},
        Mineral{.name = "Salts", .density = 2.16f, .scale = 0.20f, .albedo = rgb(230, 224, 217), .roughness = 0.40f, .metalness = 0.0f, .hardness = 2.5f},
        Mineral{.name = "Exotic", .density = 13.0f, .scale = 0.90f, .albedo = rgb(115, 31, 217), .roughness = 0.12f, .metalness = 0.80f, .hardness = 9.0f},
    };
    return table;
}

}
