# FBX → geometry → meshpack

FBX is an input format for the existing Eltanin asset pipeline. It does not create a second asset system:

```text
3ds Max → FBX → Assimp → geometry::Asset → meshpack::Asset → assembler
```

## Authoring contract

- FBX scene units are read from the file and converted to engine metres.
- FBX axis metadata is evaluated by Assimp; Eltanin keeps a right-handed OpenGL mesh and CCW winding.
- Every FBX object/node that owns geometry becomes a named geometry/meshpack **entry**.
- Every 3ds Max material name used by that object becomes a named geometry **surface**.
- Object transforms (including the 3ds Max pivot compensation) are baked into
  authored tile coordinates. FBX entries use the assembler's zero origin, matching
  existing LWO tiles and preventing the pivot from being applied twice.
- Position, normal, first UV channel and triangle indices are imported. Missing normals are generated.
- Animations, bones, sockets, collision and FBX LOD groups are intentionally ignored in v0.1.

Recommended 3ds Max preparation:

1. Set honest System Units and export them in the FBX file; do not compensate scale by eye.
2. Place each module pivot on its Eltanin grid anchor.
3. Give every exported object and every material slot a stable unique name.
4. Reset unintended object scale before export. Intentional object rotation and scale are baked into the entry geometry.
5. Export triangulated geometry with normals, smoothing data and UV channel 1.

## Meshpack sidecar

Place the FBX and its sidecar on the same asset shelf, for example:

```text
assets/Eltanin/meshes/tiles/armour_tiles.fbx
assets/Eltanin/meshes/tiles/armour_tiles.fbx.meshpack
```

Example `armour_tiles.fbx.meshpack`:

```text
{
    "tiles/armour_tiles",
    "Eltanin/meshes/tiles/armour_tiles.fbx",
    "Eltanin::mech",
    [
        {"body",  {"Eltanin::hull", [{"albedoMap", "STEEL4.JPG"}]}},
        {"trim",  {"Eltanin::hull", [{"albedoMap", "pewter2.bmp"}]}},
        {"glass", {"rmmr::one_sided_glass", [{"albedoMap", "debug06.jpg"}]}}
    ]
}
```

The first value must match the resource unit registered by the story. The second value is the asset-root-relative FBX path. Part keys must match FBX material names exactly. Texture filenames must already exist in the selected texpack.

Register it beside the existing LWO packs:

```cpp
const auto armourTiles = with<resource::Assets>::add_meshpack_fbx_loader(
    context,
    resource::Unit::Name::from("", "tiles/armour_tiles"),
    item<resource::meshpack::LoaderFbx>{
        .file = "Eltanin/meshes/tiles/armour_tiles.fbx.meshpack",
        .geometry = {},
        .pending = {},
    });
```

The assembler reference remains unchanged: pack `tiles/armour_tiles`, entry equal to the authored FBX object name.

## Verification

`Raidenmamare_fbx_tests` creates and round-trips three FBX fixtures through Assimp and the production CPU loader:

- flat tile: metre scale, pivot, normals and UV;
- wedge: triangle winding and normal orientation;
- multi-material module: object name plus `body`, `trim`, and `glass` material slots.

Run:

```bat
cmake --build build/ninja-msvc-release --target Raidenmamare_fbx_tests
build\ninja-msvc-release\modules\raidenmamare\Raidenmamare_fbx_tests.exe
```

These fixtures prove the engine contract. Before an artist pack is accepted, repeat the same three cases with files exported by the team's actual 3ds Max/FBX plug-in version, because Autodesk exporter versions can encode pivots differently.
