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
