#pragma once

#include <rmmr/resources/geometry.q1.h>

namespace rmmr::resource::geometry::assimp {

    struct LoadedMesh {
        builders::geometry::CpuPresentation cpu;
        vector<Asset::Entry> entries;
        vector<Asset::Surface> surfaces;
        vector<Asset::Mount> mounts;
        umap<string, EntryId> entryCatalog;
        vector<umap<string, SurfaceId>> surfaceCatalogs;
        vector<SurfaceId> primitiveSurfaces;
    };

    // CPU-only Assimp import used by geometry::Loader. FBX nodes become named
    // geometry entries; node and pivot compensation transforms are baked into
    // assembler-space vertices.
    auto load(const filepath&) -> optional<LoadedMesh>;

}
