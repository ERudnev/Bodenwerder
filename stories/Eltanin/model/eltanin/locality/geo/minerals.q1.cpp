#include <eltanin/locality/geo/minerals.q1.h>

#include <base/types/common_types.h>

namespace eltanin::locality::geo {

using base::common_types::rgb;

const vector<Mineral>& Mineral::table() {
    // .scale / meltKelvin / tintKelvin / glowKelvin / sinter must match rock.frag.glsl and boulder.frag.glsl
    static const vector<Mineral> table{
        Mineral{.name = "Ice", .density = 0.92f, .scale = 0.08f, .albedo = rgb(217, 235, 242), .sinter = rgb(56, 168, 255), .roughness = 0.25f, .metalness = 0.0f, .hardness = 1.5f, .meltKelvin = 273.0f, .tintKelvin = 220.0f, .glowKelvin = 900.0f, .sootMul = 0.85f},
        Mineral{.name = "Olivine", .density = 3.32f, .scale = 0.25f, .albedo = rgb(64, 89, 38), .sinter = rgb(42, 58, 25), .roughness = 0.72f, .metalness = 0.0f, .hardness = 6.5f, .meltKelvin = 2160.0f, .tintKelvin = 850.0f, .glowKelvin = 1400.0f, .sootMul = 0.08f},
        Mineral{.name = "Pyroxene", .density = 3.28f, .scale = 0.28f, .albedo = rgb(56, 46, 38), .sinter = rgb(36, 30, 25), .roughness = 0.75f, .metalness = 0.0f, .hardness = 6.0f, .meltKelvin = 1850.0f, .tintKelvin = 850.0f, .glowKelvin = 1350.0f, .sootMul = 0.08f},
        Mineral{.name = "Feldspar", .density = 2.62f, .scale = 0.22f, .albedo = rgb(191, 184, 173), .sinter = rgb(138, 132, 124), .roughness = 0.68f, .metalness = 0.0f, .hardness = 6.0f, .meltKelvin = 1470.0f, .tintKelvin = 780.0f, .glowKelvin = 1200.0f, .sootMul = 0.10f},
        Mineral{.name = "Clay", .density = 2.20f, .scale = 0.45f, .albedo = rgb(140, 107, 71), .sinter = rgb(98, 74, 48), .roughness = 0.88f, .metalness = 0.0f, .hardness = 2.0f, .meltKelvin = 1780.0f, .tintKelvin = 700.0f, .glowKelvin = 1100.0f, .sootMul = 0.10f},
        Mineral{.name = "Carbonaceous", .density = 1.80f, .scale = 0.35f, .albedo = rgb(20, 18, 15), .sinter = rgb(16, 14, 12), .roughness = 0.92f, .metalness = 0.0f, .hardness = 2.5f, .meltKelvin = 3900.0f, .tintKelvin = 520.0f, .glowKelvin = 900.0f, .sootMul = 0.45f},
        Mineral{.name = "Iron", .density = 7.87f, .scale = 0.55f, .albedo = rgb(140, 133, 128), .sinter = rgb(196, 186, 176), .roughness = 0.32f, .metalness = 1.0f, .hardness = 4.0f, .meltKelvin = 1811.0f, .tintKelvin = 800.0f, .glowKelvin = 1200.0f, .sootMul = 0.06f},
        Mineral{.name = "Nickel", .density = 8.91f, .scale = 0.55f, .albedo = rgb(158, 153, 140), .sinter = rgb(210, 204, 186), .roughness = 0.30f, .metalness = 1.0f, .hardness = 4.0f, .meltKelvin = 1728.0f, .tintKelvin = 800.0f, .glowKelvin = 1180.0f, .sootMul = 0.06f},
        Mineral{.name = "Sulfides", .density = 4.61f, .scale = 0.40f, .albedo = rgb(115, 89, 38), .sinter = rgb(168, 132, 58), .roughness = 0.48f, .metalness = 0.55f, .hardness = 4.0f, .meltKelvin = 1460.0f, .tintKelvin = 700.0f, .glowKelvin = 1100.0f, .sootMul = 0.10f},
        Mineral{.name = "Oxides", .density = 5.17f, .scale = 0.38f, .albedo = rgb(89, 46, 31), .sinter = rgb(64, 32, 22), .roughness = 0.62f, .metalness = 0.20f, .hardness = 6.0f, .meltKelvin = 1870.0f, .tintKelvin = 850.0f, .glowKelvin = 1300.0f, .sootMul = 0.12f},
        Mineral{.name = "BaseMetals", .density = 8.40f, .scale = 0.50f, .albedo = rgb(184, 115, 56), .sinter = rgb(232, 154, 78), .roughness = 0.38f, .metalness = 1.0f, .hardness = 3.0f, .meltKelvin = 1358.0f, .tintKelvin = 700.0f, .glowKelvin = 1100.0f, .sootMul = 0.07f},
        Mineral{.name = "PGM", .density = 21.0f, .scale = 0.70f, .albedo = rgb(140, 143, 148), .sinter = rgb(196, 200, 210), .roughness = 0.22f, .metalness = 1.0f, .hardness = 4.5f, .meltKelvin = 2041.0f, .tintKelvin = 900.0f, .glowKelvin = 1450.0f, .sootMul = 0.06f},
        Mineral{.name = "RareEarths", .density = 7.00f, .scale = 0.42f, .albedo = rgb(102, 115, 97), .sinter = rgb(74, 84, 70), .roughness = 0.55f, .metalness = 0.35f, .hardness = 5.0f, .meltKelvin = 1290.0f, .tintKelvin = 750.0f, .glowKelvin = 1150.0f, .sootMul = 0.10f},
        Mineral{.name = "Actinides", .density = 15.0f, .scale = 0.48f, .albedo = rgb(71, 77, 56), .sinter = rgb(52, 56, 40), .roughness = 0.45f, .metalness = 0.70f, .hardness = 6.0f, .meltKelvin = 1405.0f, .tintKelvin = 750.0f, .glowKelvin = 1150.0f, .sootMul = 0.10f},
        Mineral{.name = "Salts", .density = 2.16f, .scale = 0.20f, .albedo = rgb(230, 224, 217), .sinter = rgb(248, 250, 252), .roughness = 0.40f, .metalness = 0.0f, .hardness = 2.5f, .meltKelvin = 1074.0f, .tintKelvin = 600.0f, .glowKelvin = 1000.0f, .sootMul = 0.22f},
        Mineral{.name = "Exotic", .density = 13.0f, .scale = 0.90f, .albedo = rgb(115, 31, 217), .sinter = rgb(168, 64, 255), .roughness = 0.12f, .metalness = 0.80f, .hardness = 9.0f, .meltKelvin = 4200.0f, .tintKelvin = 1600.0f, .glowKelvin = 2600.0f, .sootMul = 0.10f},
    };
    return table;
}

}
