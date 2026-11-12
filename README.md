# <img src="cod3recomplogo3.png" alt="COD3RECOMP-ENHANCED logo" width="600">


**Call of Duty 3 (Xbox 360) on PC, with the fixes and improvements I want.**

This is my enhanced fork of CoD3Recomp, now integrated with upstream **v0.5.851** ([PR #2](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/pull/2), merged November 12, 2026). My main focus is the campaign, not a random pile of half-finished ideas.

The game itself is **NOT INCLUDED**. You need your own Xbox 360 version of Call of Duty 3. Legal shit, I know.

If you want the technical write-up, [here's the formal README](docs/README-FORMAL-BACKUP.md).

## Credit where it's due

**[FrankHUN88453/CoD3Recomp](https://github.com/FrankHUN88453/CoD3Recomp) - [hovg-N8/CoD3Recomp](https://github.com/hovg-N8/CoD3Recomp) - [my fork](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED)**

Frank started the recompilation project and PC renderer. hovg-N8 built extensively on top of it, including upstream v0.5.851 now integrated here. I didn't make their work and I'm not going to pretend I did.

The upstream project already has major campaign improvements, multiplayer features, bots, killstreaks, HDR options, performance changes, and more. **Those are NOT my additions.**

And to be clear, **I HAVE NOT TESTED MULTIPLAYER.** hovg-N8's fork has multiplayer functionality, but I'm working on the campaign, so I'm not going to claim I've personally verified it.

I've kept the older upstream READMEs in [docs/UPSTREAM-README.md](docs/UPSTREAM-README.md), and their big development history is in [docs/PATCH-NOTES.md](docs/PATCH-NOTES.md). The upstream credits and history are still there if you care.

## MAIN CHANGES

- Added **experimental blood decals and adjustable blood clouds with selectable presets**. F11 - Gameplay - Blood & Gore. 
- Added **modern COD-style hitmarkers**, with sound and customization. F11 - Gameplay.
- Added **M1 Garand partial reload**.
- Added **Increased sprint speed**. 3x longer sprint and 40% faster recovery time, F11 - Gameplay, Toggle
- **Fixed three broken buttons:** RELOAD, BINOCULARS, and OBJECTIVES.
- Added **mouse and keyboard button icons/prompts** to the UI.
- Added **REAL mouse support for the menus**. Hovering highlights the right option, clicking actually selects it, and clicking empty space does nothing. **I've tested it in Single Player, other menus, and the title flow.**
- Added a native **Quit Game?** confirmation when pressing **Esc at the main menu**, with **Quit** and **Cancel**. This uses the game's own dialog and pointer paths rather than a separate Windows popup.
  - **Small limitation:** The quit confirmation is available from the main menu, not the initial **PRESS ENTER** screen. Press Enter to reach the menu before using Esc to quit.
- Added experimental blood-decal projection filtering and rendering diagnostics.

## Blood & Gore presets

Open **F11 → Gameplay → Blood & Gore** in the campaign and choose a **Blood Preset**.

| Preset | Blood amount | Particle density | Decal lifetime | Max decals | Splat size | Cloud size | Cloud opacity |
| --- | ---: | --- | --- | ---: | ---: | ---: | ---: |
| MINIMAL | 50% | Low | 12 seconds | 24 | 0.75× | 0.75× | 0.75× |
| NORMAL | 80% | Medium | 30 seconds | 64 | 1.00× | 1.00× | 1.00× |
| ENHANCED | 120% | High | 90 seconds | 256 | 1.15× | 1.15× | 1.10× |
| BRUTAL | 200% | Ultra | Permanent | 4,096 | 1.40× | 1.50× | 1.35× |

All four presets enable blood, decals, and blood physics and use the same blood color (RGB 0.30, 0.018, 0.012). Permanent decals still count toward the decal limit.

The manual sliders and toggles remain available. The selector shows **Custom** whenever the current values differ from all four presets. Choosing **Custom** leaves the current values in place. Opening the config or reloading a checkpoint does not silently overwrite your current values with a preset.

### Bloodstain Style

Open **F11 → Gameplay → Blood & Gore → Bloodstain Style** to choose how blood looks after it lands on a surface.

- **Enhanced (Default)** uses alpha-masked splatters and directional streaks with uneven opacity, irregular edges, holes, and satellite droplets.
- **Legacy Simple** uses the original solid-color decals.

Switching styles updates stains already in the scene. It preserves Blood Spray Style, your gore preset, and all manual blood settings. The existing `blood_stain_variation` key still saves the choice (`1` for Enhanced, `0` for Legacy Simple). If the key is missing, the game uses Enhanced.

### Blood Spray Style

Choose **Blood Spray Style** under **F11 → Gameplay → Blood & Gore**.

- **Classic** is the default and uses the original explosive particle spray. Existing INI files without a spray-style setting use Classic.
- **Directional (Experimental)** biases spray in the impact direction and varies the mist and particle spread, speed, size, and lifetime.

The style is saved separately as `blood_spray_style` (`0` for Classic, `1` for Directional). Switching styles keeps the selected gore preset, and selecting a gore preset keeps your spray style.

Note: the selected blood preset still applies with Directional spray; it changes the spray pattern only, not the preset values. All four gore presets are compatible with both Classic and Directional blood.

## Known menu bugs — still need fixing

The mouse now points at actual menu entries. These issues have been tracked:

- **Sensitivity sliders can keep changing after one click.**
- ~~Go from the title screen into the menus, then back to **PRESS ENTER**, and **Enter stops working**.~~ (Crossed off the active list after the latest menu update; retained here for reference.)

The brightness and sensitivity issues remain open. The crossed-out title-gateway item is retained as a historical report. Mouse hover and clicks have been playtested successfully in the Single Player flow and the main menu.

See the [menu preview status](docs/MENU-POINTER-PREVIEW.md) and [bug log with reproduction/source leads](docs/MENU-SLIDER-BUGS.md). The slider fixes are deferred for a small follow-up patch.

## November 2026 update: upstream integration and performance

Upstream **v0.5.851** was integrated into `main` through [PR #2](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/pull/2). Merge commit: `a3df8f01261314ed3c1a997e648d9809e0274a50`; vetted integration and follow-up fixes landed with the upstream merge and follow-up cleanup.

The original, working pre-integration fork is **still backed up** on GitHub: [`backup/main-before-upstream-20261112`](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/tree/backup/main-before-upstream-20261112).

Both Windows Release executables built successfully with FFmpeg enabled, and **19 automated checks passed**. Those checks cover native pointer handling, prompts, hitmarkers, blood simulation/GPU rendering, and weapon/menu integration.

### The 6–15 FPS playtest issue: configuration, not a demonstrated merge regression

A separate clean playtest installation initially ran Forest and The Island at extremely low frame rates. Controlled comparisons using the same warmed `d5503e3` executable and graphics settings isolate the cause to configuration and runtime state, not a demonstrated renderer regression from the upstream merge itself.

| Campaign map | Watch memory writes OFF | Watch memory writes ON |
| --- | ---: | ---: |
| Forest | 9.8 median FPS | 60.0 median FPS |
| The Island | 14.4 median FPS | 60.0 median FPS |

Forest index-buffer handling dropped from roughly **38.5 µs to 0.12 µs per draw** in the controlled comparison. The old fork (`b22188d`) and stock upstream also reproduced severe slowdowns with memory watch enabled, which makes the issue much more about the per-install configuration state than a clean merge regression.

**If a fresh installation runs at unexpectedly low FPS, check F11 → Watch memory writes before assuming the renderer is broken.** Texture-pool optimisation was also different between installations, which can affect the result if the initial game setup differs.

## Blood decals: the disappearing-blood incident

Earlier versions exhibited intermittent disappearing blood, most notoriously on **Chambois**. Extensive diagnostics confirmed that blood could be generated and drawn, but they never established a definitive root cause in the renderer or data path.

Following the v0.5.851 integration and correction of the separate playtest's `watch_writes` setting, the user reported that blood **remained visible on the ground on The Island and Chambois**, including after reloads and checkpoints.

**Scope of verification:** Persistence across every campaign map, both Permanent Decals settings, and all checkpoint/reload conditions has **not** been established. Do not misrepresent passing synthetic checks as proof of full reliability.

The existing experimental blood simulation, visuals and diagnostics remain in the fork. Further speculative renderer rewrites are **on hold**.

## Roadmap (planned, not implemented)

### Wave Defense vertical slice

A standalone single-player wave-survival mode is planned, reusing existing campaign systems where possible. **For the vertical slice, launch and map selection will live in the F11 overlay**, not a new standalone menu.

Claude's read-only feasibility audit found existing host-side soldier spawning, entity tracking, overlay/HUD, and campaign map-loading mechanisms. **Repeated hostile Axis spawning, independent AI behavior, and custom wave logic still need design work before any implementation is approved.**

### BRUTAL COD3 ideas (future exploration only)

**None of the following are implemented or approved for immediate engineering work.** They are concepts to evaluate *after* existing integration checks and Wave Defense priorities:

- **Always Ragdoll on Death:** First audit whether the original engine provides usable ragdoll/death physics. Do not assume a built-in capability or commit to a custom physics rewrite.
- **Expanded gore:** More responsive blood sprays, directional hit effects, additional wound/environmental effects, with performance and visual checks.
- **Dismemberment:** A much later possibility, potentially requiring character mesh, animation and gameplay changes.
- **Movement/combat experiments:** Slide and boot-kick mechanics, subject to separate feasibility and gameplay review.

The BRUTAL COD3 name is a possible future direction, **not a claim that these features ship today**.

## Building this thing (Windows)

You'll need your **own Xbox 360 game dump**, not files from this repo. The upstream build needs **CMake 3.20+**, **Ninja**, **Clang**, and **Visual Studio Build Tools / Windows SDK**. Clang is required for the hosted toolchain and the runtime compile path.

Put your own `default.xex` in `CoD3RecompLib/private/default.xex`. From the repo root, run:

```powershell
.\scripts\recompile.ps1 -BuildTools -BuildLib -BuildHost
```

Then run the built `CoD3.exe` from its output directory. On first launch, setup can use a disc image or an extracted game folder. To point it straight at an ISO, run this **from the executable's folder**:

```powershell
.\CoD3.exe --iso "D:\path\Call of Duty 3.iso"
```

These commands come from the existing upstream build script and README. I haven't freshly verified a clean build on every machine, so I won't claim that every setup is verified. More details are in the upstream docs and build scripts.

## AI disclosure, just being clear

**This project was assisted with Codex and playtested by me.** You're free to dislike AI-assisted development, but I'd rather be transparent about my tools than pretend I didn't use them. Also this README is not an apology for the result; it's just the truth.

I still decide what goes into my fork, and I actually play the damn game.

Original game assets are not provided. Upstream code and third-party components keep their own attribution and licensing requirements. This README doesn't magically relicense anybody else's work.

This is an experimental fork, not a polished, bug-free release. I'm improving a game I wanted to play on PC and documenting the bugs instead of hiding them.
