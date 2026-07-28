# Doctrine ↔ C++ (watch / reflex debt)

After the debt wave: patterns applied only where they fit; not a full sanitize.

| Aspect | Doctrine | C++ runtime |
|--------|----------|-------------|
| Something | `sprite: #Sprite?` | Entity; roles share Id |
| Session | `world: anchor<World>`; `player: #Something?` | matches; menu Camera2d sync |
| Player | `feature of Something`; always sprite_idle/scale; `!steer`/`!tryFire` | appear from always; sync via mom |
| Shot | `feature of Something`; always sprite_*/scale; `!advance->>advanceMotion` | appear from always; sync/cull via mom |
| Alien | `feature of Something`; always sprite_*/scale; `?sprite_index` | appear from always; sync via mom |
| Fleet / Volley | whole `!march` / `!schedule` | left whole |
| Saucer / Bunker | draft / partial | Saucer doctrine only |
| Bootstrap | `>` factories | creates Something+sprite then extends role |

Public `>` stay factories. Reaction tails stay Internals.
