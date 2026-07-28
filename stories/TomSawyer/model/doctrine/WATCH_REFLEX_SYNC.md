# Doctrine ↔ C++ (watch / reflex debt)

After the debt wave: patterns applied only where they fit; not a full sanitize.

| Aspect | Doctrine | C++ runtime |
|--------|----------|-------------|
| GameObject | `sprite?` + `hitpoints`; `>takeDamage` / `?alive` | shared Id HP pool for all role features |
| Session | `start_lives` / `wave_clear_steps`; scene wiring; free `note*` | matches; Playfield `contains`/`install` |
| Player | custody Gun; `hit_half`; `!steer`/`!tryFire` | matches |
| Gun | dual limiter; `>fire`; `!cool(~World)` | matches |
| Shot | motion + `hit_half`; hits in `advanceMotion` | no fire factories (Gun/Volley own spawn) |
| Alien | `?worldPos`; `hit_half`; no local alive | `Alien::worldPos`; HP on mom |
| Fleet / Volley | march constants; `Volley.>fire` + `!schedule` | matches |
| Bootstrap | `newMatch` + wave/reset/clear | matches |
| Saucer / Bunker | removed from live doctrine | — |

Public `>` stay factories. Reaction tails stay Internals.
