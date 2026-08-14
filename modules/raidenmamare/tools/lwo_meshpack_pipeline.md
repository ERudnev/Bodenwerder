# LWO → meshpack pipeline

Repeatable agent/human path after LightWave edits. **Do not** hand-parse `.lwo` binary for surfaces/textures — use the script next to this file.

## What “update the meshpack” means

Sidecar `*.lwo.meshpack` tells the engine:

| field | meaning |
|---|---|
| unit name / library | shelf identity (`Eltanin::armourDecor`, …) — must match `add_meshpack_lwo_loader` |
| `.lwo` path | kit-relative geometry file |
| texpack | usually `Eltanin::mech` (folder `assets/.../textures/mech`); `-` = no albedo array |
| **parts** | every **geometry surface name** → material + texture **layer filename** |

Loader rules (see `meshpack.q1.cpp`):

- Parts keys must cover **all** Assimp surface names from the LWO (and must not invent extras that never appear).
- Default lit material is `rmmr::lit_textured` with one `albedoMap` = **basename** of a file that exists in that texpack directory.
- LAYR names become meshpack **entries** (e.g. `p1111_nose_decor`); mounts JSON `tempMesh.entry` points at those names.

## Tools

| script | role |
|---|---|
| `lwo_surf_textures.py` | Inspect LWO3 CLIP/STIL + SURF Image Maps + LAYR; print or **write** `.meshpack` parts |
| `lwo_strip_envl.py` | If Assimp chokes on envelopes: strip ENVL/ENVS/ENVD from the LWO |

Paths below assume repo root `DAQL/`.

## Agent checklist (“обнови мешпак X”)

1. **Inspect**
   ```bat
   python modules/raidenmamare/tools/lwo_surf_textures.py assets/Eltanin/meshes/.../foo.lwo
   ```
   Check: surfaces ↔ texture basenames; layers ↔ expected entry names; pivots if placement matters.

2. **Confirm textures are on the shelf**  
   Files must live under the texpack directory (for mech kits: `assets/Eltanin/textures/mech/`). Catalog loader indexes by **filename**; renaming a texture in LW without adding the file → runtime refuse.

3. **Write / refresh the sidecar**
   ```bat
   python modules/raidenmamare/tools/lwo_surf_textures.py assets/Eltanin/meshes/.../foo.lwo --write-meshpack
   ```
   - Rewrites only the **parts** list from LWO surfaces (first Color Image Map → `albedoMap`).
   - Keeps existing header (`name`, `library`, `lwo` path, `texpack`) when the `.meshpack` already exists.
   - New file: defaults `name`/`library` from path heuristics; texpack `Eltanin::mech` unless `--texpack` / `--no-texpack`.

4. **Surfaces with no Image Map**  
   Script prints a warning and leaves `MISSING_TEXTURE` (or refuses `--write-meshpack` unless `--allow-missing`). Fix in LW (add map) or edit that one part by hand / pass `--albedo-fallback pewter2.bmp`.

5. **If Assimp fails to load**  
   ```bat
   python modules/raidenmamare/tools/lwo_strip_envl.py assets/Eltanin/meshes/.../foo.lwo
   ```
   Then re-run inspect / write.

6. **Smoke**  
   Run the story that registers the pack; refuse lines about missing surface / missing texpack layer mean parts or filenames still wrong.

## Manual overrides (rare)

- Wrong Image Map picked when a SURF has several: edit that one `albedoMap` in the `.meshpack`.
- Untextured / solid material: e.g. attachments use `Eltanin::typeSolid` and `texpack: "-"`.
- Do **not** invent surface names that are not in the LWO SURF list Assimp will keep.

## Example

```bat
python modules/raidenmamare/tools/lwo_surf_textures.py assets/Eltanin/meshes/fittings/mounts/armour_decor.lwo
python modules/raidenmamare/tools/lwo_surf_textures.py assets/Eltanin/meshes/fittings/mounts/armour_decor.lwo --write-meshpack
```
