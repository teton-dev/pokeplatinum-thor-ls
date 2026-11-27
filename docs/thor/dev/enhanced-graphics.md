# Enhanced graphics: plan (started 2026-11-23, ahead of the order below)

Optional effects drawn by our own shaders on top of what the DS renders: cloud shadows, sun lighting and god rays,
better weather, water reflections, rim lighting, shadows, lamp bloom, blowing leaves, fireflies. **This is the last
step of the fork's roadmap**, after Gen 5 battles (`gen5-battles.md`), HeartGold/SoulSilver support and Renegade
Platinum support. The owner chose on 2026-11-23 to start early, with three effects first (below). Baseline for everything: light-weight,
optimized and performant on the Thor's GPU; each effect off by default, measured with libntr's frame timings.

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

## Findings

- **Sprites and the objects just behind them** (2026-11-12): the overworld uses the Z-buffer (`Camera_SetProjection`
  sets `gBufferMode` to Z; only some cut scenes keep the W-buffer). Twinleaf's town sign is a 3D model whose board's lower
  edge is about 3 view units nearer the camera than the player's sprite on the tile in front, so it covers the top of the
  cap. The DS snaps that edge to its 256x192 pixels (about one row of overlap, mostly the cap's outline); at the Thor's
  resolution the overlap is larger. The port nudges the overworld's sprites toward the camera for the depth test only
  (`billboard.c`, `SIM_SetSpriteDepthBias`, `libntr_sprite_depth.patch`): 8 units at their tops, nothing at their feet,
  so tall grass still covers feet. An earlier attempt (a W-buffer depth patch) didn't apply to the overworld and was
  dropped.

## First effects (decided 2026-11-23)

Order: infrastructure + time-of-day, then sprite shadows, then water, then leaves.

1. **Time-of-day lighting, colour grading, sun shadows** (sprites and objects).
2. **Reflective water** (bodies and puddles).
3. **Blowing leaves** (BW2's intro scene) on some routes in Platinum and HGSS.

### Architecture

- **Where code lives:** `subprojects/libntr` is a checkout of upstream's; changes go in new
  `subprojects/packagefiles/libntr_enhanced_*.patch` files listed in `libntr.wrap` (never edit the checkout).
- **One combined pass** attached beside `G3EdgeMarking()` in `sim_main.cpp`, on the 3D framebuffer only (grading after
  the 2D composite would tint text boxes). It reads the edge pass's attribute texture (polygon ID, 16-bit depth), runs
  once per new frame (`G3SIM_DrawSerial()`, so HGSS at 30 Hz isn't graded twice), keeps the premultiplied alpha
  convention, and covers `sG3Viewport` (widescreen width). The attribute texture exists only without MSAA
  (`sSamples == 0`): check Android's default before relying on depth.
- **Shaders** are written for `#version 420 core` and rewritten to ES 3.2 by `SIM::PrepareShaderSource`
  (`sim_shaders.cpp`); new files are registered there. Keep to what survives that rewrite.
- **Game to libntr API:** one small, game-agnostic call (`SIM_SetEnhancedScene`: time of day, sun direction, weather,
  map kind), set from `src/port/`; HGSS supplies its own values.
- **Tagging polygons:** `billboard.c` already brackets overworld sprite drawing with `SIM_SetSpriteDepthBias`; reuse
  that bracket to tag sprite polygons. Cheap geometry beats screen-space effects: a sprite's quad sheared along the sun
  direction onto the ground is its shadow; the same quad mirrored at the feet and masked to water is its reflection.
- **Colour grading** builds on `libntr_color_adjust.patch` (the Color Balance sliders), not a second pass.

### Findings so far

- The game already reflects sprites: `renderDetails->hasReflection` and `TileBehavior_IsPuddle` /
  `TileBehavior_IsShallowWater` in `src/map_object_move.c` (about lines 411 and 525-562). Water and puddle tiles are
  known from tile behaviour, so the reflection mask can come from map data rather than guessing from textures. Check
  how far its own reflection already goes before adding ours.
- Time of day: `TimeOfDayForHour` (`include/rtc.h`); the light vectors are set per map by the game
  (`NNS_G3dGlbLightVector`): find where before adding sun direction of our own.
- BW2's leaves: find the intro's particle resources in pokebw2's intro overlay; draw them ourselves in a light pass
  (textures read from the imported BW2 ROM at runtime, never committed), not through BW2's particle code.

### Progress

- **Slice 1 done on the desktop (2026-11-23): the pass and time-of-day light.** `libntr_enhanced_grade.patch` adds
  `SIM_SetEnhancedScene(r, g, b, vignette)`, a setting (*Settings > Display > Time-of-day light*, `EnhancedGrading`,
  off by default) and `G3EnhancedGrade()` beside `G3EdgeMarking()`: one triangle whose fragment shader reads nothing
  (`g3_enhanced_fragment.glsl`); the blend (colour times what is there) does the multiply, so there is no copy of the
  frame. It writes the colour output only (the second one holds the polygon IDs and depth), runs once per drawn frame
  (`G3SIM_DrawSerial()`), over `sG3Viewport` (so full width in widescreen), and draws nothing while the scene is
  neutral. The game side is `src/port/enhanced_graphics.c` (called at the end of `Widescreen_Update`): the light
  curve by hour, outdoor maps only (`MapHeader_IsOutdoors`), neutral in menus and battles, so HGSS supplies its own
  curve and map test. Not yet checked: the Thor (Android/GLES) and its cost; the curve's colours are a first guess.
- **Gotchas:** Lake Verity's map type is `MAP_TYPE_CAVE`, so it gets no light (`MapHeader_IsOutdoors` is only town/city
  and outdoors); tests fix the hour with `POKEPLATINUM_ENHANCED_HOUR` (`tests/scripts/enhanced_grade.txt`). The
  attribute texture (depth, IDs) exists only without anti-aliasing; this pass doesn't need it, but the depth-based
  effects (shadows' masks, reflections) will: check Android's default before building them.
- **Sun shadows of sprites done on the desktop (2026-11-23), `libntr_enhanced_shadows.patch`, *Display > Sun shadows*.**
  `BillboardList_Draw` tells libntr which billboards cast (`SIM_SetSpriteShadow`; `Billboard.castsShadow` is set where the
  billboard is made, `ov5_021ECF1C`, from the object's `hasShadow`). Inside the sprite depth-bias bracket,
  `G3SIM_SubmitPolygon` also emits the polygon sheared along the ground from its lowest point on screen (x by the sun's
  side, y by the ground's depth, at the feet's depth), queued as a deferred item with the batch's texture; it is drawn
  after the translucent items in the sprite's own shape (alpha-tested, one dark colour), each pixel once (stencil), no
  depth write. The game's hour curve (`SetSun`) gives the side (left in the morning, right in the evening), the length
  (long when low) and the strength (none at night). The blob shadow is hidden while the sun's is shown
  (`EnhancedGraphics_SunShadowsShown`, checked in the two blob effects' per-frame update in `ov5_021F134C.c`, so dusk and map entry follow). Checked at 1x and at 4x internal resolution: the shadow
  has the sprite's outline and its texels stay sharp. **Not done: shadows of map props** (trees, signs, buildings are 3D
  models, a separate method).
- **Water reflections done on the desktop (2026-11-23), `libntr_enhanced_water.patch`, *Display > Water reflections*.**
  The tile data is the mask: `EnhancedGraphics_DrawWaterMask` (called at the end of the field's 3D render, `fieldmap.c`)
  finds the tiles that are `HasReflectiveSurface` or `IsSurfable` around the player (cached while the player's tile and
  the map stay), merges them into runs and draws each as a quad under `SIM_SetPolygonTag(1)`: libntr keeps those polygons
  instead of drawing them. At the end of the frame (`G3EnhancedWater`, before the grade): (1) the quads are drawn into
  an R8 mask against the scene's depth (shared renderbuffer, no depth write), (2) one fragment per DS pixel marches up
  the mask to the first non-water pixel (the shore, at most 40 DS pixels), (3) a copy of the 3D viewport, (4) at full
  resolution over the water's bounds only, each water pixel reads the copy a whole number of DS pixels above its shore
  (mirrored about it, with a ripple in whole DS pixels, fading with distance), `texelFetch` only, never filtered, so the
  pixel art stays pixels at 4x. Skipped with anti-aliasing on, with no water tiles, and for frames shown twice.
  `SIM_WATER_DEBUG=1` paints the mask blue and the pixels with a shore red (`tests/scripts/enhanced_water.txt`,
  Canalave's canal under the bridge: the planks are mirrored in the water).
- **Limits and to-dos:** the march goes straight up the screen, so only a shore *above* the water (the far bank) is
  mirrored: a canal's side walls, or a lake whose far shore is off the screen, show nothing (right for a planar
  mirror: only things beyond the water can show in it). The game's own reflection billboards (`hasReflection`, on
  puddle/river/reflective tiles) are still drawn and now double with ours: hide them while the setting is on. Water
  that isn't a surf/reflective tile (shallow water, Lake Verity's low water) isn't masked. Tile heights come from
  `TerrainCollisionManager_GetHeight`; check bridges and multi-level maps. **GPU cost not measured** (software GL on
  the desktop; use libntr's frame timings on the Thor), **Android not built or tried.**
- **Next:** blowing leaves (BW2's intro scene): find the intro's particle resources in pokebw2, draw them in our own
  light pass; shadows of map props; hide the game's reflection billboards; tune the light curve on the Thor.
- **Checked 2026-11-23:** all three at 16:9 (Canalave, `SecondaryScreen=true Widescreen=true`): the shadows, the mirror
  and the tint cover the whole width, no doubled blob. **Not checked:** puddles (the game's own reflection billboards
  on puddle/river tiles are probably doubled by ours; test on a rainy route and decide whether to skip
  `ov5_021F1800` while the setting is on), indoor reflective floors (the mask doesn't check the map type), Android (four
  new shaders and an edited `g3_fragment.glsl` were not built or run as GLSL ES), GPU cost (the scene copy blits the
  whole viewport: crop it to the water's columns if the Thor's timings ask for it).
- **Sun shadows corrected (2026-11-23), `libntr_enhanced_shadow_grid.patch`.** The first version sheared the sprite's
  quad, which started the shadow at the bottom of the rectangle (empty rows under the feet, so it didn't meet them) and
  cut the texels into slanted blocks. Now one item per sprite: `Texture::FeetFraction` finds the lowest opaque row of the
  sprite's part of the texture (the texture keeps its alpha on the CPU), the shadow starts there, and the fragment shader
  lays the sprite down texel for texel: each shadow row is one row of the sprite's picture (rows squeezed by the sun's
  flattening, never filtered or cut across) shifted sideways by a whole number of texels per row, so the grid stays the
  sprite's and it only stretches. The rectangle is taken before clipping, so a sprite half off the screen keeps its
  shape. Curve: sideways reach 0.12 per hour from noon, flattening 0.24 + 0.03 per hour from noon. Looked at at 4x
  (8:00, 12:00, 17:00); not on the Thor.
- **Sun shadows of the 3D scene (2026-11-23), `libntr_enhanced_sun_shadows_3d.patch`, same *Display > Sun shadows* setting.**
  Trees, signs, houses and cliffs cast shadows, and the shadows their models carry are no longer drawn. First tried and
  dropped: drawing the map props again flattened onto the ground (`MapPropManager_Render2`): the props of a chunk are few
  (8 in Twinleaf's neighbour, none on Route 201); the trees and the sign are part of the land model, and flattening that
  would flatten the terrain. Now a screen-space pass, so it needs no model knowledge: `EnhancedGraphics_SetCamera`
  (fieldmap.c, after the camera is set, before the map is drawn) gives libntr the game's projection and the direction toward
  the sun in view space; at the vblank, at the DS's resolution (one fragment per DS pixel, so shadow edges are DS pixels),
  each pixel's view distance is rebuilt from the depth the 3D shader left in the attribute texture (`d = m32 / (ndcz +
  m22)`), and a ray of 32 steps of 1.5 view units is marched toward the sun; a pixel with something nearer than the ray's
  point (and not thicker than 40 units) on the way is in shadow. A full-resolution multiply (the blend, so no frame copy)
  darkens those pixels in the sprites' shadow tone. It runs before the water pass, so reflections include the shadows.
  The sprites are flagged in bit 0 of the attribute's red channel (`spriteFlag`; libntr's batch is flushed when the
  sprite bracket changes) and neither cast nor receive here: they keep their own shadows. The sun's direction in the world
  (`SetSun`) leans the same way as the sprites' shadows (checked at 9:00, 12:00, 16:00, at 3x and 4x, and at 16:9).
  `SIM_SUNSHADOW_DEBUG=1` paints the mask, `=2` the reconstructed distances as grey bands (flat ground comes out as even
  bands: the check of the depth maths). The built-in shadows (translucent, alpha 12 of 31 or less, untextured or
  4-colour: the sign's, the houses') are dropped only while the land and props are drawn (`EnhancedGraphics_LandDrawing`
  brackets them in fieldmap.c) and the sun's shadows are shown, so footprints, ripples and the like are untouched.
  **Known:** the dark rings at the foot of the trees are painted into the tree models' opaque textures and stay; a roof's far slope
  and tall things' near-edge ground can show thin hatching where the ray grazes a surface; shadows from things off the screen
  (a tree just above the top edge) are missing at the edges; the `fmt=0 alpha=12` family wasn't told apart from the
  houses' shadow, all of it goes with the rule above. Not on the Thor; cost unmeasured (32 depth reads per DS pixel, about
  1.5M reads a frame).
- **House shadow gaps fixed (2026-11-23)**: the march's mask had holes where a ray slipped between two steps of a thin edge, and
  specks where it grazed a surface. The march now reaches farther (40 steps of 2 units, 120 units of thickness it still counts),
  and a clean-up pass at the DS's resolution (`g3_sunshadow_clean_fragment.glsl`) closes holes (a pixel joins a shadow when 6 of
  the 9 around it are in it) and drops specks (stays only with 4 of 9); pixels with nothing to shade (sprites, empty) are left
  alone. (A lesson: an NPC that walks turns her back to the camera between runs, and her dark hair looks like a black bug.)
- **Sun highlights (2026-11-23), `libntr_enhanced_highlights.patch`, *Display > Sun highlights*, off by default.** At the DS's
  resolution, the surface normal is rebuilt from the depth (differences of the neighbours' view positions, from the side with
  the smaller jump), kept only where it agrees at one and two pixels' reach (flat) and isn't the ground (the world's up in view
  space comes with the camera). Only **buildings** get it: they carry a polygon ID the ground, trees and the sign don't (8 in
  Twinleaf; the DS gives them IDs to outline them; the highlight pass reads it from the attributes, `SIM_HIGHLIGHT_DEBUG=2`
  paints the IDs, `=1` the normals). Light = specular glint (half vector of sun and eye, exponent 48) + a lift where the face
  looks at the sun + a fresnel sheen at a graze, in the sun's colour (white-warm at noon, orange when low; strength follows the
  shadows' curve) and not where the sun shadows' mask says shadow. It **brightens the pixel's own colour** (`dst * (1 + light)`,
  one blend, no frame copy), so the texture's detail and black outlines stay as drawn; sprites get a **rim** of the sun's colour
  added on their outermost pixels on the sun's side (so it shows on their outlines). Two cheap passes: one DS-res fragment per
  pixel (about 10 attribute reads), then the full-res apply (nearest, discards where zero). Looked at 9:00, 12:00, 16:00 at 3x
  and 4x, at 16:9 with the other effects on.
  **Not done / known:** the glint is soft and broad because a flat face has one normal (it moves with the hour, but only
  a face near the sun's mirror angle flashes); the walls' sheen is slight; other buildings' polygon IDs (Poké Centers,
  gyms) weren't checked beyond Canalave's; no Thor run, cost unmeasured.
- **Thin objects' shadows (2026-11-23)**: the posts by Canalave's canal threw wedges as fat as walls, because a ray passing
  anywhere within 120 units behind a surface counted as blocked. The march (`g3_sunshadow_march_fragment.glsl`) now takes
  the occluder's width into account: a ray close behind a surface (under 4 units) is blocked as before, and farther behind
  only if the surface is about that wide (`Extent`: the run of continuous-depth pixels along the ray's direction on the
  screen, up to 24 each way, times 1.5 plus 4). Posts give thin streaks with a rounded tip; houses and trees keep their
  full shadows (checked in Canalave, Twinleaf and Route 201). `tests/scripts/enhanced_posts.txt`. Sprites keep the exact
  silhouette of their own picture; everything else is shaped by what the depth buffer shows, now width-aware.
  **Follow-up (2026-11-24):** the width is measured across the screen but the gap is along the view ray, which crosses an
  upright post over width / cos(the ray's angle with the ground), so seen from above a post is deeper than it is wide and
  rays through its body were rejected (the right post's streak had a fat tip). The test now divides the extent by
  `across = sqrt(1 - dot(q, up)^2)` (at least 0.3; `upView` is shared with the highlights pass, in
  `libntr_enhanced_highlights.patch`). Both Canalave posts give even bands with rounded tips; the bridge, Twinleaf's
  houses and Route 201's trees are the same or better, and the Route 201 sign's shadow is now right. The unused `rayStep`
  is gone. `SIM_SUNSHADOW_DEBUG` also shows why a pixel is shadowed (close behind / width test / rejected).
  Re-shot at 4x with every enhanced setting on (`enhanced_posts.txt`, 16:00): shadows, reflections, highlights, haze and
  glow together look right.
- **GLES (2026-11-24, not yet on the Thor):** all 20 enhanced shaders pass `glslangValidator` as GLSL ES 3.20 (one fix: a
  `const` initialised from a runtime value in the highlights shader). The C++ side's GL calls and texture formats are
  GLES 3.0-safe. Still to do: build the APK, run it on the Thor, measure each pass's frame time at 4x and 6x.
- **Shadow edges at the internal resolution (2026-11-24, owner's decision), `libntr_enhanced_shadow_res.patch`.** The
  owner found the shadows' DS-pixel edges coarse next to the 4x-6x scenery and asked for their edges to scale with the
  resolution, sprites keeping their texel shape. This reverses the "shadows on the DS grid" rule for shadows only (the
  art itself is still never filtered). Sprites: each texel row of the shadow is cut into `scale` sub-rows and shifted by
  whole sub-texels (`shadowSubdivide` in `g3_fragment.glsl`), so a slanted edge has steps `scale` times smaller while
  every sub-cell still takes its alpha from one texel. 3D: the mask is still marched and cleaned a DS pixel at a time;
  the apply pass blends the four nearest mask texels by the pixel's place among them and shades where that passes one
  half, so DS stairs become smooth edges of small pixels (corners get a slight bevel; the shapes stay the mask's).
  No extra cost: four reads per pixel in the apply. At 1x both reduce exactly to the old output (checked: the 1x shots
  differ only where NPCs walked). Checked at 4x in Canalave (posts, the bridge's thin brace shadows stay continuous),
  Twinleaf and Route 201 (no fringe on sprites next to 3D shadows), at 16:9, and with `SIM_SUNSHADOW_DEBUG` 1-3.
  **Tried and dropped:** marching the mask at the internal resolution itself. It combs: on rough occluders (tree crowns,
  the sign) neighbouring small pixels' rays land on different march steps and width verdicts, giving 1-pixel streaks
  along the shadows' edges; measuring the width from the DS pixel's centre didn't cure it and added stray lines. It
  would also cost the square of the scale (16x the march at 4x). The 3D shadows' detail stays a DS pixel; only their
  edges are finer.
- **3D shadows from a real shadow map (2026-11-25), `libntr_enhanced_shadow_map.patch`.** The owner pointed at the post's
  shadow: a lobe along its top that the post's body doesn't explain. `SIM_SUNSHADOW_DEBUG=3` showed the whole band passed
  the march's width test: the march can't know how thick a thing is along the view (a post seen from above is deeper
  than it is wide), so each allowance only moved the error (fat wedges, then a thin shaft with a fat tip, then the lobe
  from dividing by the view angle). Now libntr keeps the casters themselves: in `G3SIM_SubmitPolygon`, before clipping
  (so what reaches past the screen's edges still casts), every opaque polygon drawn inside the land bracket
  (`SIM_SetHideBuiltInShadows`) or a new caster bracket (`SIM_SetSunCasters`: the game sets it around signposts and
  mailboxes, which are map objects drawn as models by `ov5_021F10E8.c`, after the land) is kept as triangles of clip x,
  y, w and texture s, t; sprites, the water mask and translucent polygons are not. Each run of them is told its batch's
  texture and wrapping when the batch is flushed (`G3SIM_CasterBatchEnd`). Once a frame (`G3SunShadowMap` in
  `sim_main.cpp`) the vertices go back to view space (x / m00, y / m11, z = -w), then into an orthographic view along
  the sun fitted to them, and are drawn into a 2048x2048 depth texture, texture by texture, leaving out clear texels (tree
  crowns are boxes with round pictures; untextured, their shadows were boxes). The receive pass
  (`g3_sunshadow_receive_fragment.glsl`) puts each DS pixel's surface in the sun's view and compares; bias 1 unit. It
  writes the march's mask, so the soft-edged apply and the highlights are unchanged; the clean-up is skipped for it (it
  erased the bridge cables' one-pixel shadows, and the map has none of the march's holes or specks;
  `SIM_SUNSHADOW_CLEAN=1` puts it back). The casters start afresh with each 3D frame, where libntr clears the 3D buffer
  (`SIM_Begin3DCommand` after a swap: `G3SIM_ResetCasters`), so fast-forward's several game frames to a drawn one don't
  pile up in the map. **The 3D's sun now follows the sprites' shadows** (`EnhancedGraphics_SetCamera`): with no limit on
  length, the map showed the old `sSunWorld` gave 3D things shadows about 2.3 times as long per unit of height as the
  sprites' at 16:00 (the march had hidden it: it stopped at 82 units). The sun is now worked out from the sprites' lean
  (`sSpriteShear`, what `SIM_SetEnhancedSun` gets) through the camera: (4/3) shx east and, northward, what reaches shy up
  the screen per unit of height (view y of world z). A post's shadow is now as long as the player's beside it, the sign's
  as the NPC's; the highlights and atmosphere use the same sun.
  `SIM_SUNSHADOW_METHOD=march` brings the old march back (the code is kept until the map is proven on a device);
  `SIM_SUNSHADOW_DEBUG=4` and `=5` show the map's depth and each pixel's own (they match on lit ground).
  Checked at 4x: both Canalave posts give bands as wide as the post with no lobe; the bridge's braces, cables and truss
  cast; Route 201's trees cast crown-shaped shadows and the sign casts; Twinleaf's house (its roof's west slope in shadow
  at 9:00) and mailbox; no acne on flat ground at 9:00, 12:00 or 16:00; 1x, 16:9 and camera tilt look right. Not
  checked: a walk while shadows are on (the test's jump spots leave the player where walks are blocked), fast-forward on
  a device. The canal's water now takes the shadows of the walls and the building to its
  west. Cost: one CPU pass over at most 18k vertices, a 2048x2048 depth clear and draw, one lookup per DS pixel, about a
  batch's worth of draw calls; likely cheaper than the march but unmeasured on the Thor and Duo (16 MB of depth texture).
  **Not done:** casters the game doesn't submit (off-screen chunks it culls) still cast nothing; a scene with far chunks
  loaded spreads the map's texels thinner (they are fitted to all casters); other model objects (boulders, Cut trees,
  berry trees) are not in the caster bracket yet.
- **Jubilife on the Duo (2026-11-25), `libntr_enhanced_jubilife.patch`.** The owner's report from the Retroid Pocket
  Duo (Eterna 9100057): Jubilife couldn't hold 60 fps, a building's shadows looked out of place, and the rim on running
  sprites warbled. (1) The frame-timings overlay and *Settings > Advanced* now time each enhanced pass on the GPU
  (`gpuEnhancedMs`: shadows, highlights, water, atmosphere, grade, glow; `gpu3dMs` is the scene alone now) and the shadow
  map's CPU side. On the Duo in Jubilife (all effects on) the 3D took 1.4 ms, the top screen 4.3, the bottom 2.6,
  atmosphere 2.7, shadows 2.1, highlights 1.7, glow 0.5: about 15.3 of 16.7 ms, 57 fps. Cuts: the shadow map is 1024x1024
  (pixels are looked up a DS pixel at a time, 2048 bought nothing); the clean-up pass is left out with the map (the mask
  the later passes read is `sSunMaskTex`); the atmosphere's shafts look each of their four points up in the shadow map
  when it was drawn this frame (4 reads a pixel instead of 36 marched); the rim pass stops on one read of the DS
  highlight texture away from sprites. After: 60 fps, game 60/60, 6.7 ms of the frame spare; GPU 3D 2.1, top 4.3,
  bottom 2.6, shadows 1.2, highlights 2.1, atmosphere 0.8, glow 0.5 (about 13.6 ms). Highlights are now the costliest
  pass. (2) Jubilife's Pokemon Center roof had dotted self-shadowing on the slope away from the sun: the map's casters are
  drawn with a slope-scaled polygon offset (2, 4). (3) The rim was worked out on the DS grid fixed to the screen, which a
  running sprite crosses in small steps; it is now found in the full-resolution apply (`g3_highlight_apply_fragment.glsl`):
  a sprite's pixel whose neighbour one sprite pixel (pixelStep) toward the sun isn't the sprite's, so it moves with the
  sprite's own pixels. Test: `tests/scripts/enhanced_jubilife.txt`. Not checked on the device: the rim while running
  (the owner's eye), other heavy towns (Hearthome, Veilstone), the Thor.
- **Planar water reflections (2026-11-25), `libntr_enhanced_water_mirror.patch`.** The owner found the water reflections
  slow and full of the usual screen-space faults (only what is on the screen, mirrored about wherever the water's edge
  is on the screen, smears); cube maps were weighed and passed over (they suit far surroundings, not a flat pond with its
  bank a tile away). The water now shows a real mirror image: the land and buildings' polygons (the shadow map's
  capture, now with vertex colours, `G3SIM_CASTER_STRIDE` 9 floats, kept whenever reflections or sun shadows want them,
  by day and night: `SIM_SetLandDrawing` around the land, the signposts' bracket ungated) are drawn mirrored about the
  water's level with the game's own projection (`SIM_SetEnhancedCamera`, every field frame) into a viewport-sized image,
  over the water's bounds only, textured as the DS draws them, nothing at or below the level; each water pixel reads its
  own place in it, with the old whole-DS-pixel ripple. No copy of the frame, no shore search. **The level:** the game's
  water tiles (the mask) stand at the bank's height, which on Route 204's sunken ponds is 24 units (a tile and a half)
  above the water; mirrored about that, the reflections landed on the grass. The level is now the flat captured polygons
  inside the tiles' footprint at or below that height, the height with the most area (the water's own surface).
  `SIM_WATER_METHOD=screen` brings the old mirror back; `SIM_WATER_DEBUG=3` shows the mirror image over the water's
  bounds. On the Duo by Route 204's ponds the water pass took 0.6 ms (the first version, before the level fix). Checked
  on the desktop: Canalave's canal (the bridge's underside, braces and pillars), Lake Verity's cliffs (no smears), Route
  204's ponds (grass, path, the trees' undersides; the fence north of the left pond is hidden behind the pond's wall from
  every point of the water, as it would be). **Known:** one level per view; what the game doesn't draw isn't mirrored;
  people and Pokemon keep the game's own reflections.
  **Later the same day (owner's choice):** the flip about the water's far edge on the screen is the default again, as the
  eye expects of this view: the shore is found at full resolution above each pixel (on the DS grid the mirror jumped a DS
  pixel at a time as the camera glided) and only the part of the frame the mirror can read is copied.
  `SIM_WATER_METHOD=planar` draws the planar mirror, now about the bank (the tiles' height) and of things standing only
  (no ground, nothing at or below the bank): a true mirror of a sunken pond showed its far bank as a second plane of
  grass hiding the fences and trees. Not yet re-checked on the Duo or the Thor.
- **Steady shadows and off-screen casters (2026-11-25, Windows session), `libntr_enhanced_shadow_stable.patch`.** The
  owner's report: the 3D shadows warbled while walking and surfaces shaded themselves in shaky patterns; shadows of
  things just off the screen were missing. Causes found: (1) the shadow map was refitted to the casters every frame in
  view space, so its texels slid and stretched under the scene as the camera moved; (2) the receive pass gave a yes or
  no per DS pixel on a grid fixed to the screen, then thresholded, so edges stepped a DS pixel at a time as the world
  slid under it; (3) a single unfiltered lookup with a constant bias left acne (dashed lines along the canal's walls,
  combs on the house shadow's edge) that changed with every texel shift; (4) the map's props (houses, per-chunk models)
  are box-test culled by the game (`map_prop.c`), so off-screen ones never reached libntr's caster capture.
  Fixes: the map is **held still in the world**: the game passes the world's origin in view space every field frame
  (`SIM_SetEnhancedWorldOrigin`, from `EnhancedGraphics_SetCamera`), the map's corner is snapped to whole texels of a grid
  anchored there, and its size changes only in 64-unit steps (grown at once, shrunk below 3/4); the receive pass pushes
  the looked-up point off its surface along the normal rebuilt from the depth (half a texel to two, more where the sun
  grazes), bias 0.5, and **filters** it (four hardware comparisons, `sampler2DShadow` through a sampler object bound for
  that pass only, so the atmosphere's plain reads are untouched); the mask is soft (0 to 1) and the apply pass eases it in
  over a small band (`smoothstep(0.2, 0.8)`, partial multiply), so edges move smoothly with the scene; and the mask is
  looked up at **twice the DS's resolution** when the internal resolution is 2x or more (a shadow a DS pixel wide, a cable
  or post, otherwise flickered; `SIM_SUNSHADOW_MASK_SCALE=1` keeps the DS's, for timing; the highlights pass reads it with
  `shadowScale`). Off-screen props: with sun shadows on, the props the box test rejects are drawn anyway when their
  position is within about half a screen of the view (`EnhancedGraphics_NearView`); libntr keeps them as casters before
  clipping and the clip drops the rest, so nothing extra is shown. Batches with casters but no visible vertices now set up
  their texture too (`G3SIM_CastersPending`), so a tree crown off the edge casts its round shadow, not its box.
  **Checked on the Windows desktop build (RTX 5070 Ti, 4x):** `SIM_SUNSHADOW_LOG=1` over the east and north-south walks
  (`enhanced_walk_shadows.txt`, `_ns.txt`): the sun's axes and the map's span bit-identical every frame while the world
  origin moved ~150 units (the grid's phase is 0 by construction, so the log's evidence is the constant axes and span);
  the masks (`SIM_SUNSHADOW_DEBUG=1`) compared with the old code on the same walk: the old house-shadow edge is a comb
  whose specks change every frame and the canal walls carry dashed acne; the new edges are smooth and the walls clean in
  every still. Whether the warble is gone in motion is for the owner to judge on the device (stills can't show it).
  Device-like settings run headless with all effects but water: 16:9 (`wide4`), 1x, 3x, camera tilt; no breakage seen
  (the 1x/3x/tilt runs end indoors, so they check the passes' scaling, not shadows outdoors). Canalave posts, Route 201 at 9:00 and 16:00
  (crowns round, the sign casts), Jubilife (no caster overflow with the near-view props; shadows from the west edge come in
  from off-screen). Specks remain inside tree crowns (foliage shading itself; invisible in colour). All 19 enhanced
  shaders pass `glslangValidator` as GLSL ES 3.20. **Not checked:** the Duo/Thor (cost: the receive pass now reads about
  13 texels per mask texel on 4x the texels: time it with the per-pass overlay, A/B with `SIM_SUNSHADOW_MASK_SCALE=1`).
  **Known:** the map covers every loaded caster (span 1152 units in Canalave: a texel is 1.1 units); fitting it to the
  view's neighbourhood would sharpen it. The planar water mirror (`SIM_WATER_METHOD=planar`) no longer gets the off-screen
  props (only sun shadows ask for them). The owner also mentioned an optional lightweight AA (SMAA 1x-like): not done; it
  needs the owner's call against the "AA stays off, never filter pixel art" rule (an AA restricted to 3D polygon edges via
  the attribute texture's IDs and depth would respect it). **The flicker at map borders (fixed the same day):** the owner
  saw the whole picture flicker walking between Jubilife City and Route 204 on the Duo. `enhanced_walk_204.txt` with
  `SIM_SUNSHADOW_LOG=1`: on the frame the border is crossed one caster thousands of units away stretched the map's fit for
  that frame (span 1536 -> 11072, then 1216), so every shadow smeared at once. The fit now takes only casters within 1600
  units of the camera (the loaded map around the player is ~1500 across); the span then stays 1536 across the crossing.
  The old per-frame fit had the same jump.
  **The Duo's frame rate (2026-11-25, 9100084-86, every effect on, Route 204 by the ponds, daytime):** at "match the
  screen" (6x on its 1920x1080) 54 fps, GPU-bound (swap 8.6 ms): GPU 3D 1.3, top screen 4.3, bottom 2.6, shadows 1.4,
  highlights 2.2, water 0.9, air 0.9, grade 0.5, glow 0.5 ms. The 2D compositor fetched all ten layers at every screen
  pixel: it now skips empty or disabled ones (`layerMask`; `SIM_G2_ALL_LAYERS=1` for the old way; output identical where
  runs caught the same frame), top 4.3 -> 3.9 ms, 56 fps. At 5x: 60/60 with 7 ms idle (shadows 1.3, highlights 1.5,
  water 0.6, air 0.6 ms). Timing on a device: `adb shell setprop debug.eterna.hour 13` fixes the hour the light follows
  (empty to clear), so the sun's passes run at night. Then 2D-only screens composited at the DS's 256x192 and blown up
  (`SIM_G2_FULL_RES=1` for the old way; menus identical): bottom screen 2.6 -> 0.1 ms; and the sun shadows' multiply done
  in the highlights' pass (`SIM_SEPARATE_APPLY=1` apart; at most 2 levels of rounding apart): at 6x 60/60 with 7.4 ms
  idle (9100087). **The white screen / water soft reset (fixed):** `FindWater` (`src/port/enhanced_graphics.c`) asked the
  game for tiles outside the loaded map matrix (x 32 of the 32-wide house); the game's lookup answers that with
  `CommManager_SetCommError`, and with the field's link session (`COMM_TYPE_PARTY`) running that became the error reset:
  a white screen on loading into a small map with water on, and every headless test with water. Found with gdb (MSYS2's
  `mingw-w64-ucrt-x86_64-gdb`, the window kept off-screen by `WindowX=-6000` in the run's `sim_config.ini`). The search now
  stays inside the matrix.
  **Reflections going out while walking (the band above the screen, same day, owner's choice over a planar fallback):**
  bursts of captures on the Duo showed the right pond on Route 204 mirroring the trees above it, and its reflection
  going out whenever walking slid those trees off the top of the screen (the screen mirror only has what is on the
  screen). With enhanced water on (and no MSAA), the 3D is now drawn 40 DS pixels taller than the screen: the 3D
  framebuffer has a band of rows above the screen's half, the 3D viewport covers screen and band (`g3_vertex.glsl`'s
  `bandScale` puts the screen's rows exactly where they were), libntr's clipper moves its top plane up to match
  (`SIM_SetClipTop`; the box test with it, so props just past the top edge are drawn too), the compositor samples only
  the screens' rows (`tex3dScaleT`), and every pass but the water's keeps to the screen. The water's mask, shore search
  (`searchHeight`) and copy of the frame reach into the band; the reflection fades out over the band's last 8 rows.
  Edge marking runs over the band too: the screen's top row had been outlined against the clear plane, which the mirror
  showed as a line across the reflection. `SIM_WATER_BAND=0` turns it off. Checked on the desktop
  (`enhanced_ponds_top.txt`: the pond's reflection stays at z 733-735, where it went out without the band; gone at 736,
  where the trees are past the band too), Jubilife with every effect pixel-identical band on and off, a widescreen Gen 5
  battle with water on. Cost: 21% more 3D pixels (40 of 192 rows), not yet timed on the Duo. The 3D shadows' edges are now soft while sprite (billboard)
  shadows stay hard: the owner's call whether that mix is fine. **Found, not from this change:** with `EnhancedWater=true`,
  `enhanced_atmosphere.txt` soft-resets at the title screen (`OS_ResetSystem`), with the old shadow code too; not
  investigated yet. `SIM_SUNSHADOW_MASK_SCALE` is an environment variable, so a release APK can't A/B it without a
  `sim_config.ini` key.
- **Camera tilt (not a shader; `src/port/camera_tilt.c`)**: *Display > Camera tilt* (0, 6 or 12 degrees lower), applied
  around the field map's draw in `fieldmap.c` (the camera's angle is raised before the view matrix is made, put back after
  the draw), only for perspective cameras looking down at 30 to 75 degrees with no yaw, so scenes that set their own angle are
  untouched. Checked on Route 201 at 0, 6 and 12 (`TEST_CONFIG_PRJ="CameraTilt=1"`). Not checked: very wide scenes (far land
  chunks not loaded at the top of the screen), the Thor.
- **Atmosphere and glow (2026-11-23), `libntr_enhanced_atmosphere.patch`, *Display > Atmosphere* and *Display > Glow*
  (`EnhancedAtmosphere`, `EnhancedBloom`).** Atmosphere (`G3EnhancedAtmosphere`, after the water, before the grade): (1)
  at DS resolution, `g3_atmosphere_march_fragment.glsl` takes 4 points on each pixel's view ray, 10 to 72 view units
  (about a tile to four) above its surface, and marches each toward the sun through the depth in the attribute texture
  (the sun shadows' test, without the width check): the lit share is the pixel's shaft value (the shadow of a house or
  tree reaches up into the air); (2) at full resolution `g3_atmosphere_apply_fragment.glsl` blends a haze that thickens
  toward the top of the screen (screen height, not depth: robust in widescreen and at any tilt; the colour and share by
  hour in `sAir`, `enhanced_graphics.c`) and adds the sun's colour times the shaft value times a gain that grows as the
  sun gets low. Only over pixels the 3D wrote (the attribute alpha). Glow (`G3EnhancedBloom`, after the grade): the frame
  is blitted down to one pixel per DS pixel (a pick, nearest), a bright pass keeps what passes the hour's luminance
  threshold, two 11-tap blur passes at DS size, and the result is enlarged with bilinear filtering (a haze of light, so
  filtering is right there; the frame underneath isn't touched) and added. Game side: `SIM_SetEnhancedAtmosphere`,
  `SIM_SetEnhancedBloom`; the sun's light is now also set when only Atmosphere is on. `SIM_ATMOSPHERE_DEBUG=1` shows the
  shaft values alone. Test: `tests/scripts/enhanced_atmosphere.txt`.
- 2026-11-26, **shadows gone on some Jubilife tiles (Duo, widescreen)**: libntr kept at most 6144 caster triangles a frame
  (fixed array in `g3_handler.cpp`); Jubilife at (174, 784) to (174, 782) in widescreen needs ~6400-6800, and the rest were
  left out (`[sunshadow] more casters than 18432 vertices`). On the Duo every map shadow went out two tiles north; the
  desktop overflowed too but kept them (not explained). The buffer now grows (a `std::vector`, doubled as needed) up to
  65536 triangles; the owner confirmed on the Duo (9100097) that the building and lamp shadows stay along the street.
  Test: `tests/scripts/enhanced_jubilife_casters.txt` (widescreen, `SIM_SUNSHADOW_LOG=2` logs the caster count when it
  changes; no "more casters" lines expected).
- Device diagnostics (Android, `adb shell setprop`; the Duo's logcat keeps only error lines, so libntr also writes its
  stdout to the data directory's `app-log.txt`, the launch before in `app-log-prev.txt`): `debug.eterna.poslog 1` logs
  the map and tile at each step, `debug.eterna.sunlog 1` the shadow casters/fit, `debug.eterna.sundebug N` and
  `debug.eterna.separate 1` the desktop's `SIM_SUNSHADOW_DEBUG` / `SIM_SEPARATE_APPLY`, `debug.eterna.perflog 1` the
  frame timings (at launch), `debug.eterna.hour H` the light's hour.
