#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

#include <array>

namespace eltanin::geo {

    using namespace fqsm::api;

    // Sphere surface packed as 10 diamonds of a regular icosahedron.
    // Each diamond is two triangles sharing the (right, left) edge; local (iu, iv) is a square vertex grid.
    // Every cell of that grid is cut on u+v = 1, same diagonal as the two icosa faces.
    struct IcosaPack {
        static constexpr integer diamondCount = 10;
        static constexpr integer faceCount = 20;
        static constexpr integer shellCount = 12;
        static constexpr integer diamondsPerRow = 5;

        struct Diamond {
            integer top;
            integer right;
            integer bottom;
            integer left;
        };

        struct Slot {
            integer diamond;
            integer iu;
            integer iv;
        };

        struct Sample {
            integer diamond;
            float u;
            float v;
        };

        // One cell of the (u, v) square, cut on u+v = 1 — the icosa edge (right, left).
        // Upper bary is (top, right, left); lower is (right, bottom, left).
        struct Split {
            bool lower;
            vec3 bary;
        };

        struct Tri {
            Slot a;
            Slot b;
            Slot c;
            vec3 bary;
        };

        integer edgeBase;
        integer tessellation;

        auto edgeSegments() const -> integer;
        auto edgeVertices() const -> integer;
        auto storedCount() const -> integer;
        auto uniqueCount() const -> integer;
        auto index(Slot) const -> integer;
        auto slotOf(integer) const -> Slot;
        auto atlasSize() const -> index2;
        auto atlasCoord(Slot) const -> index2;
        auto sampleOf(Slot) const -> Sample;
        auto direction(Slot) const -> vec3;
        auto triangle(Sample) const -> Tri;
        auto upper(integer diamond, integer iu, integer iv) const -> std::array<Slot, 3>;
        auto lower(integer diamond, integer iu, integer iv) const -> std::array<Slot, 3>;

        static auto split(float u, float v) -> Split;
        static auto vertices() -> const std::array<vec3, shellCount>&;
        static auto diamonds() -> const std::array<Diamond, diamondCount>&;
        static auto locate(vec3 direction) -> Sample;
        static auto direction(Sample) -> vec3;
    };

}

namespace eltanin::geo {

    inline auto IcosaPack::edgeSegments() const -> integer {
        return edgeBase * (integer{1} << tessellation);
    }

    inline auto IcosaPack::edgeVertices() const -> integer {
        return edgeSegments() + 1;
    }

    inline auto IcosaPack::storedCount() const -> integer {
        const integer span = edgeVertices();
        return diamondCount * span * span;
    }

    inline auto IcosaPack::uniqueCount() const -> integer {
        const integer segments = edgeSegments();
        return diamondCount * segments * segments + 2;
    }

    inline auto IcosaPack::index(Slot slot) const -> integer {
        const integer span = edgeVertices();
        return (slot.diamond * span + slot.iv) * span + slot.iu;
    }

    inline auto IcosaPack::slotOf(integer linear) const -> Slot {
        const integer span = edgeVertices();
        const integer spanSq = span * span;
        const integer diamond = linear / spanSq;
        const integer remainder = linear - diamond * spanSq;
        return Slot{.diamond = diamond, .iu = remainder % span, .iv = remainder / span};
    }

    inline auto IcosaPack::atlasSize() const -> index2 {
        const integer span = edgeVertices();
        return index2{.x = diamondsPerRow * span, .y = 2 * span};
    }

    inline auto IcosaPack::atlasCoord(Slot slot) const -> index2 {
        const integer span = edgeVertices();
        return index2{.x = (slot.diamond % diamondsPerRow) * span + slot.iu, .y = (slot.diamond / diamondsPerRow) * span + slot.iv};
    }

    inline auto IcosaPack::sampleOf(Slot slot) const -> Sample {
        const float segments = static_cast<float>(edgeSegments());
        return Sample{.diamond = slot.diamond, .u = static_cast<float>(slot.iu) / segments, .v = static_cast<float>(slot.iv) / segments};
    }

    inline auto IcosaPack::direction(Slot slot) const -> vec3 {
        return direction(sampleOf(slot));
    }

    inline auto IcosaPack::split(float u, float v) -> Split {
        if (u + v <= 1.0f)
            return Split{.lower = false, .bary = vec3{1.0f - u - v, u, v}};
        return Split{.lower = true, .bary = vec3{1.0f - v, u + v - 1.0f, 1.0f - u}};
    }

    inline auto IcosaPack::upper(integer diamond, integer iu, integer iv) const -> std::array<Slot, 3> {
        return {Slot{.diamond = diamond, .iu = iu, .iv = iv}, Slot{.diamond = diamond, .iu = iu + 1, .iv = iv}, Slot{.diamond = diamond, .iu = iu, .iv = iv + 1}};
    }

    inline auto IcosaPack::lower(integer diamond, integer iu, integer iv) const -> std::array<Slot, 3> {
        return {Slot{.diamond = diamond, .iu = iu + 1, .iv = iv}, Slot{.diamond = diamond, .iu = iu + 1, .iv = iv + 1}, Slot{.diamond = diamond, .iu = iu, .iv = iv + 1}};
    }

}
