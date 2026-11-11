# Enhanced graphics: plan (last step, not started)

Optional effects drawn by our own shaders on top of what the DS renders: cloud shadows, sun lighting and god rays,
better weather, water reflections, rim lighting, shadows, lamp bloom, blowing leaves, fireflies. **This is the last
step of the fork's roadmap**, after Gen 5 battles (`gen5-battles.md`), HeartGold/SoulSilver support and Renegade
Platinum support. Nothing here is being built yet: this file records the idea, what's known now, and what to keep in
mind meanwhile so it's easy to start later.

## Order

1. Gen 5 battles in Platinum (`gen5-battles.md`, milestones 5 to 7).
2. HeartGold/SoulSilver support.
3. Renegade Platinum support.
4. **Enhanced graphics** (this file).

## The effects

Each one is its own setting (*Settings > Display > Enhanced graphics > ...*), off by default, and must work in
widescreen.

| Effect | Notes |
| --- | --- |
| Cloud shadows | Soft shadows of clouds drifting over the ground on outdoor routes. A new effect, not Eterna Forest's: that one is the game's `OVERWORLD_WEATHER_CLOUDY` overlay (below). Projected onto the 3D ground, so it's full width and follows the terrain. |
| Sun lighting and god rays | Light direction and colour from the game clock (morning, day, evening, night). God rays as a screen-space pass from the sun's position; through the trees in forests. |
| Enhanced weather | Rain, snow, sand, fog redone as 3D or full-screen effects (particles, rain streaks with depth, fog by distance), which also lets those scenes go widescreen. |
| Water reflection and refraction | Puddles (after rain) and water tiles reflect the scene and ripple; refraction for shallow water. |
| Rim (fresnel) lighting | Edge highlights on 3D models and on the billboarded overworld sprites. |
| Shadows | Characters, sprites and objects cast shadows from the sun or the lamps instead of (or along with) the game's blob shadows. |
| Street lamp bloom and shadows | Lamps glow at night (bloom) and light their surroundings, with shadows. |
| Blowing leaves | Leaves drifting across some routes. BW2 has these: take the particles and textures from the player's BW2 ROM at runtime, like the Gen 5 battle files (never commit them). |
| Fireflies | Small drifting lights in Eterna Forest and other forests, at night or in the dark. |

## What's known now

- **Renderer:** libntr (`subprojects/libntr`) emulates the DS's 2D and 3D engines and draws them with OpenGL, with its
  own GLSL shaders in `libraries/sim/src/shaders` (`g3_*` for the 3D engine, `g2_*` for the 2D layers,
  `g3_edge_*` for edge marking). The effects go in there or in a pass after it, so this is libntr work as much as
  game work; libntr is upstream's (`SilasMayes/libntr`), so plan how much goes there and how much stays in this fork.
- **Why Eterna Forest is 4:3:** its tree shadows are the game's `OVERWORLD_WEATHER_CLOUDY` weather, a scrolling 2D
  overlay sized for 256 pixels. `ClearWeather()` in `src/port/widescreen.c` only lets the clear weathers go wide,
  so the forest (and rain, snow, sand, fog, Flash's darkness) drops back to 4:3. Redoing those as shader effects
  (or stretching the overlays where that looks right) is what lets them go widescreen.
- **What the shaders need from the game:** the 3D scene's depth, the camera, which polygons are ground, water,
  models or sprites, the time of day, the weather, the map (outdoor, forest, cave, town) and where lamps are. The
  DS's 3D engine doesn't say what a polygon is, so the game has to tell libntr (a small port API, like
  `SIM_SetWidescreenScene`), or the effects key off textures and map data.
- **Platforms:** the Thor (Android, OpenGL ES, a phone GPU) is the target; budget for it, not for the Mac.

## Keep in mind meanwhile

- Keep new port code that touches rendering (widescreen, Gen 5 battles' 3D) going through small, named hooks
  between the game and libntr rather than ad-hoc changes in the renderer, so later passes have places to attach.
- HeartGold/SoulSilver shares libntr: effects written against libntr's renderer should work in both games, with
  each game supplying its own map and time data.
- Record here any rendering finding from the other work that matters for this (depth, clipping, 2D overlays).
