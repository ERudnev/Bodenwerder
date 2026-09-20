#pragma once

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::geo {

    using namespace fqsm::api;

    // Face catalog; ids = texpack layer index. Order matches doctrine/geo/facies.md.
    enum class Facies : integer {
        Snow,
        Glacier,
        DirtyIce,
        VolatileFrost,
        Hydrate,
        Dunite,
        Peridotite,
        Pyroxenite,
        Komatiite,
        Basalt,
        Gabbro,
        Andesite,
        Granite,
        Rhyolite,
        Obsidian,
        Anorthosite,
        ClayPan,
        Laterite,
        Arenite,
        Evaporite,
        Carbonate,
        Chondrite,
        Tholin,
        Bitumen,
        IronMetal,
        NickelMetal,
        Sulfide,
        SulfurPlains,
        SO2Frost,
        Fumarole,
        Hematite,
        Magnetite,
        DesertVarnish,
        BaseMetal,
        Porphyry,
        PGMLag,
        REELaterite,
        Actinide,
        Pahoehoe,
        Scoria,
        Pumice,
        SilicaSinter,
        RegolithMafic,
        RegolithFelsic,
        Breccia,
        Pegmatite,
        Exotic,
        Caliche,
    };

    inline auto pack(Facies surface, Facies below) -> std::uint16_t {
        return std::uint16_t(std::uint16_t(surface) | (std::uint16_t(below) << 8));
    }

}
