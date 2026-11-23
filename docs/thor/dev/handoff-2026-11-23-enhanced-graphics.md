# Handoff: enhanced graphics, 2026-11-23

State at the end of the session. Everything below is **uncommitted and desktop-only**: nothing has been built for
Android, nothing has run on the Thor, and no GPU cost has been measured. The plan and progress log are in
`docs/thor/dev/enhanced-graphics.md`; the feature list is in `docs/thor/features.md`.

## What exists (each is its own off-by-default setting)

| Effect | Setting | libntr patch | Port code |
|---|---|---|---|
| Time-of-day tint / vignette | Display > Time-of-day light | `libntr_enhanced_grade.patch` | `src/port/enhanced_graphics.c` |
| Sprite sun shadows (on the feet, DS texel grid) | Sun shadows | `..._shadows.patch`, `..._shadow_grid.patch` | `billboard.c`, `Billboard.castsShadow` |
| 3D scenery sun shadows (trees, signs, houses) | Sun shadows | `..._sun_shadows_3d.patch` | `EnhancedGraphics_SetCamera`, `LandDrawing` in `fieldmap.c` |
| Sun highlights (buildings, sprite rim) | | `..._highlights.patch` | |
| Water reflections (mask from tile data, shore march, mirror) | Water reflections | `..._water.patch` | `EnhancedGraphics_DrawWaterMask` in `fieldmap.c` |
| Atmosphere (haze by screen height, sun shafts) | Atmosphere | `..._atmosphere.patch` | `SetAir` in `enhanced_graphics.c` |
| Glow (bloom on bright pixels) | Glow | `..._atmosphere.patch` | `SetAir` |
| Camera tilt 0/6/12 degrees | Camera tilt | none | `src/port/camera_tilt.c`, `CameraTilt=` in `sim_config_prj.ini` |

libntr pass order in `SIM_Render`: edge marking, sun prepare, sun shadow, highlights, water, atmosphere, grade,
bloom. Passes are skipped with MSAA or a W-buffer.

Patch order in `subprojects/libntr.wrap` (`diff_files`): pacing, grade, shadows, water, shadow_grid,
sun_shadows_3d, highlights, atmosphere (the last has been appended after the others). **Another session has
overwritten these entries before: re-check the line before committing.**

## Open problems, most important first

1. **Right Canalave post's shadow has a bulge near its tip.** The left post is now a thin even streak (the width
   test in `g3_sunshadow_march_fragment.glsl` measures the run horizontally, `Extent(qDs, scene, ivec2(1, 0))`).
   The right post still bulges; a stricter continuity threshold (1.2 instead of 2.5) did not fix it and made the
   bridge pillar ragged, so it was reverted. Cause unknown: only the mask was inspected. Next: dump the attribute
   depth around that post (`SIM_SUNSHADOW_DEBUG=1|2`) and see what other geometry the ray meets (paving edge, a
   second prop, the railing). I did not re-shoot the final non-debug posts image, nor the left post over water.
2. **Thor/Android is untested.** The new shaders (march, apply, bright, blur, bloom) have never compiled under GLES
   on the Thor. Check GLSL `#version 420` use against the GLES path other libntr shaders take, then measure frame
   time per pass at 4x and 6x internal resolution (bloom and atmosphere run at DS resolution; the apply passes at
   full resolution).
3. **Glow detects by brightness only.** White walls can glow, and no real lamp or window light has been checked
   (night in Jubilife, Veilstone, Canalave's lighthouse). There is no light-source mask yet.
4. **Shafts are subtle.** Tuned down after a washed-out first look (bloom must not divide by the grade tint; shaft
   gain was too high). Gain is `0.05 + 0.10 * low` in `SetSun`; the table is `sAir[]` in `enhanced_graphics.c`.
5. **Unchecked cases:** indoor reflective floors, doubled reflections from the game's own water billboards (not yet
   hidden), camera tilt with far land chunks, a small `rayStep` variable now unused in `g3_sunshadow_march_fragment.glsl`.

## Not started

- **Blowing leaves** (BW2 intro particles on some HGSS/Platinum routes).
- **Puddles** (rain reflections), and hiding the game's own reflection billboards.
- Prop shadows beyond posts, trees' texture-painted base rings (kept on purpose).
- Android build and on-Thor tuning; commit and PR (no upstream PRs until it is fully working and debugged).

## Workflow notes

- `tools/docker/desktop.sh build` **before** `desktop.sh test`: the test command does not rebuild.
- Env: `POKEPLATINUM_ENHANCED_HOUR=<0-24>`, `TEST_CONFIG` (libntr `[General]` keys, space separated),
  `TEST_CONFIG_PRJ` (game ini), `SIM_SUNSHADOW_DEBUG=1|2` (shadow mask), `SIM_ATMOSPHERE_DEBUG=1` (shaft values).
- Scripts: `tests/scripts/enhanced_{grade,posts,shadows_town,water,atmosphere}.txt` (see `tests/README.md`).
  Canalave water and posts: `jump 33 50 730`. Run with `--from turtwig`.
- 4x shots: `SCREEN=1280x1600`, `TEST_CONFIG="InternalResolutionScale=4 ... WindowWidth=1024 WindowHeight=1536"`,
  crop the top 1024x768. Shots land in `~/thor-dev/test-shots/<test name>/` and are overwritten per run.
- Random-walking NPCs change facing between runs (dark hair): compare the same frame before calling a diff a bug.
- Verify libntr patches: `git archive HEAD` of `subprojects/libntr` into a fresh dir, apply every patch in
  `diff_files` order, `diff -rq` against the checkout (ignore `.orig`, `.meson-subproject-wrap-hash.txt`). In zsh run
  it under `bash -c`; `rm -rf $VAR` is blocked, so use a fresh dir name each time.
- Pixel art must never be filtered: integer DS-pixel offsets, `texelFetch`, nearest blits. AA stays off; 4 to 6x
  internal resolution replaces it. Everything must stay widescreen-safe (`xScale`).

## Files touched this stretch (uncommitted)

libntr (via patches): `g3_atmosphere_march/apply_fragment.glsl`, `g3_bloom_bright/blur/apply_fragment.glsl`,
`g3_sunshadow_march_fragment.glsl`, `sim_main.cpp`, `sim.h`, `sim_shaders.hpp/.cpp`, `sim_config.h/.c`,
`sim_settings.cpp`. Port: `src/port/enhanced_graphics.c`, `include/port/enhanced_graphics.h`,
`src/port/camera_tilt.c`, `include/port/camera_tilt.h`, `src/port/gui_settings.cpp`, `src/port/widescreen.c`,
`src/overlay005/fieldmap.c`, `src/billboard.c`, `tools/docker/desktop.sh`, docs and tests. The git status also
shows unrelated modified files (party menu, start menu) from other sessions: do not commit those with this work.

## Commit notes

Commit as the owner, end with `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, no `Claude-Session:`
lines or links, never commit ROMs or saves. Split into logical commits (libntr patches + wrap, port code, docs and
tests).
