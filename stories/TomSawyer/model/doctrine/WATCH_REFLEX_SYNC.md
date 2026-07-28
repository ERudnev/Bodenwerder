# Doctrine ↔ C++ (watch / reflex debt)

After the debt wave: patterns applied only where they fit; not a full sanitize.

| Aspect | Doctrine | C++ runtime |
|--------|----------|-------------|
| GameObject | `sprite: #Sprite?` | `gameObject.h` / `.cpp`; roles share Id |
| Session | `world: anchor<World>`; `player: #GameObject?` | matches; menu Camera2d sync |
| Player | `feature of GameObject`; always sprite_idle/scale/bank; `!steer`/`!tryFire` | appear from always; sync via mom |
| Shot | `feature of GameObject`; always sprite_*/scale; `!advance->>advanceMotion` | appear from always; sync/cull via mom |
| Alien | `feature of GameObject`; always sprite_*/scale/bank; `?sprite_index` | appear from always; sync via mom |
| Fleet / Volley | whole `!march` / `!schedule` | left whole |
| Saucer / Bunker | draft / partial | Saucer doctrine only |
| Bootstrap | `>` factories | creates GameObject+sprite then extends role |

Public `>` stay factories. Reaction tails stay Internals.
