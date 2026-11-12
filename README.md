# <img src="cod3recomplogo3.png" alt="COD3RECOMP-ENHANCED logo" width="600">


**Call of Duty 3 (Xbox 360) on PC, with the fixes and improvements I want.**

This is my enhanced fork of CoD3Recomp, now integrated with upstream **v0.5.851** ([PR #2](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/pull/2), merged November 12, 2026). My main focus is the **single-player campaign**: fixing annoyances and adding a few modern touches.

The game itself is **NOT INCLUDED**. You need your own Xbox 360 version of Call of Duty 3. Legal shit, I know.

If you want the technical write-up, [here's the formal README](docs/README-FORMAL-BACKUP.md).

## Credit where it's due

**[FrankHUN88453/CoD3Recomp](https://github.com/FrankHUN88453/CoD3Recomp) - [hovg-N8/CoD3Recomp](https://github.com/hovg-N8/CoD3Recomp) - [my fork](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED)**

Frank started the recompilation project and PC renderer. hovg-N8 built extensively on top of it, including upstream v0.5.851 now integrated here. I didn't make their work and I'm not going to pretend I did.

The upstream project already has major campaign improvements, multiplayer features, bots, killstreaks, HDR options, performance changes, and more. **Those are NOT my additions.**

And to be clear, **I HAVE NOT TESTED MULTIPLAYER.** hovg-N8's fork has multiplayer functionality, but I'm working on the campaign, so I'm not going to claim I've personally verified it.

I've kept the older upstream READMEs in [docs/UPSTREAM-README.md](docs/UPSTREAM-README.md), and their big development history is in [docs/PATCH-NOTES.md](docs/PATCH-NOTES.md). The upstream credits and history are preserved.

## MAIN CHANGES

- **Fixed three broken buttons:** RELOAD, BINOCULARS, and OBJECTIVES. Yes, basic buttons. They were busted.
- Added **mouse and keyboard button icons/prompts** to the UI.
- **NEW** Added **REAL mouse support for the menus (preview)**. Hovering highlights the right option, clicking actually selects it, and clicking empty space does nothing. **I've tested it in Single Player, other menus, and the in-mission menus, and it works.** This uses the game's own menu controls rather than fake D-pad movement. Some sliders and title-screen navigation still have bugs, so don't mistake this for every last menu being perfect.
- **NEW** Added a native **Quit Game?** confirmation when pressing **Esc at the main menu**, with **Quit** and **Cancel**. This uses the game's own dialog and pointer paths rather than a separate Windows popup. The prompt has been confirmed working in a live playtest. Implementation: [commit `5da18f4`](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/commit/5da18f444e9470e19382196a57875132afd48c2d). [Native-menu groundwork and tests](docs/NATIVE-MENU-DIALOG-GROUNDWORK.md).
  - **Small limitation:** The quit confirmation is available from the main menu, not the initial **PRESS ENTER** screen. Press Enter to reach the menu before using Esc to quit.
- Added **modern COD-style hitmarkers**, with sound and customization. F11 - Gameplay.
- Added **M1 Garand partial reload**, because throwing away the whole clip behavior every time wasn't what I wanted.
- Added **experimental blood decals and adjustable blood clouds**. F11 - Gameplay - Blood & Gore. **Recent hands-on tests report persistent blood on The Island and previously problematic Chambois; this does not establish that every map/checkpoint scenario passes.**
- Added experimental blood-decal projection filtering and rendering diagnostics.

## Known menu bugs — still need fixing

The mouse now points at actual menu entries. These issues have been tracked:

- **Sensitivity sliders can keep changing after one click.**
- ~~Go from the title screen into the menus, then back to **PRESS ENTER**, and **Enter stops working**.~~ (Crossed off the active list after the latest menu update; retained here for reference.)

The brightness and sensitivity issues remain open. The crossed-out title-gateway item is retained as a historical report. Mouse hover and clicks have been playtested successfully in the Single Player and in-mission menus, plus other tested menus. The confirmed Single Player build and the expanded playtest executable are preserved locally. Later help-caption/button-capture refinements in this source have not been live playtested. Campaign build and focused simulated-input checks passed, but the tests missed the native slider-repeat behavior; they aren't proof that the bugs above are solved. Full gameplay cursor-capture transitions and multiplayer remain unverified.

See the [menu preview status](docs/MENU-POINTER-PREVIEW.md) and [bug log with reproduction/source leads](docs/MENU-SLIDER-BUGS.md). The slider fixes are deferred for a small follow-up patch.

## November 2026 update: upstream integration and performance

Upstream **v0.5.851** was integrated into `main` through [PR #2](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/pull/2). Merge commit: `a3df8f01261314ed3c1a997e648d9809e0274a50`; vetted integration head: `d5503e31a5b6a67deafd35604bf02419a8da4bb4`. The fork's custom menu pointer owns the campaign screens it recognizes (including the native quit confirmation), while upstream handles the others. Ownership transitions clear pending clicks. The integration also preserves the custom hitmarkers, partial M1 Garand reload, experimental blood effects and diagnostics, keyboard prompts, and existing weapon-setting IDs.

The original, working pre-integration fork is **still backed up** on GitHub: [`backup/main-before-upstream-20261112`](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/tree/backup/main-before-upstream-20261112) at commit `b22188d5067f4ec8e52bcc8e65540bb173da8eaf`. This backup is based on upstream 0.5.424 and must be retained.

Both Windows Release executables built successfully with FFmpeg enabled, and **19 automated checks passed**. Those checks cover native pointer handling, prompts, hitmarkers, blood simulation/GPU rendering, and weapon-setting compatibility. They are not a substitute for live testing of every menu, multiplayer, or gameplay path. There were no GitHub check runs attached to the PR.

### The 6–15 FPS playtest issue: configuration, not a demonstrated merge regression

A separate clean playtest installation initially ran Forest and The Island at extremely low frame rates. Controlled comparisons using the same warmed `d5503e3` executable and graphics settings isolated the dominant cause to **`watch_writes = 0`** (Watch memory writes disabled) in that playtest's INI. The original installation had memory-write tracking enabled. Enabling **Watch memory writes** restored the tested maps to **60 FPS**, without changing renderer code or other graphics settings.

| Campaign map | Watch memory writes OFF | Watch memory writes ON |
| --- | ---: | ---: |
| Forest | 9.8 median FPS | 60.0 median FPS |
| The Island | 14.4 median FPS | 60.0 median FPS |

Forest index-buffer handling dropped from roughly **38.5 µs to 0.12 µs per draw** in the controlled comparison. The old fork (`b22188d`) and stock upstream also reproduced severe slowdowns with memory tracking disabled, so the tests did **not** identify the integration as the cause. The playtest INI was backed up, then only `watch_writes = 1` was restored. The user subsequently confirmed the integrated build was playable at 60 FPS.

**If a fresh installation runs at unexpectedly low FPS, check F11 → Watch memory writes before assuming the renderer is broken.** Texture-pool optimisation was also different between installations, but the decisive comparison changed only memory-write tracking.

## Blood decals: the disappearing-blood incident

Earlier versions exhibited intermittent disappearing blood, most notoriously on **Chambois**. Extensive diagnostics confirmed that blood could be generated and drawn, but they never established a definitive renderer root cause. Earlier plans for a full renderer redesign and a campaign-wide blood survey are **no longer active work items**.

Following the v0.5.851 integration and correction of the separate playtest's `watch_writes` setting, the user reported that blood **remained visible on the ground on The Island and Chambois**, including the formerly problematic Chambois case. That is a meaningful hands-on success and the historical bug is considered **resolved in those observed sessions**, without a blood renderer rewrite.

**Scope of verification:** Persistence across every campaign map, both Permanent Decals settings, and all checkpoint/reload conditions has **not** been established. Do not misrepresent passing synthetic tests or individual maps as a universal guarantee. If a reproducible regression returns, collect the specific map, settings, and steps before reopening renderer diagnostics.

The existing experimental blood simulation, visuals and diagnostics remain in the fork. Further speculative renderer rewrites are **on hold**.

## Roadmap (planned, not implemented)

### Wave Defense vertical slice

A standalone single-player wave-survival mode is planned, reusing existing campaign systems where possible. **For the vertical slice, launch and map selection will live in the F11 overlay**, not a new native main-menu BONUS MODES entry. The first playable target is one suitable map, five hostile soldiers to start, basic wave/kill tracking, and a wave HUD.

Claude's read-only feasibility audit found existing host-side soldier spawning, entity tracking, overlay/HUD, and campaign map-loading mechanisms. **Repeated hostile Axis spawning, independent AI behavior, level-script isolation, and protection of normal campaign checkpoints/saves remain unproven or high-risk.** Do not present the mode as playable yet. A proper main-menu BONUS MODES entry can be revisited later.

### BRUTAL COD3 ideas (future exploration only)

**None of the following are implemented or approved for immediate engineering work.** They are concepts to evaluate *after* existing integration checks and Wave Defense priorities:

- **Always Ragdoll on Death:** First audit whether the original engine provides usable ragdoll/death physics. Do not assume a built-in capability or commit to a custom physics rewrite.
- **Expanded gore:** More responsive blood sprays, directional hit effects, additional wound/environmental effects, with performance and visual checks.
- **Dismemberment:** A much later possibility, potentially requiring character mesh, animation and gameplay changes.
- **Movement/combat experiments:** Slide and boot-kick mechanics, subject to separate feasibility and gameplay review.

The BRUTAL COD3 name is a possible future direction, **not a claim that these features ship today**.

## Building this thing (Windows)

You'll need your **own Xbox 360 game dump**, not files from this repo. The upstream build needs **CMake 3.20+**, **Ninja**, **Clang**, and **Visual Studio Build Tools / Windows SDK**. Clang is required here.

Put your own `default.xex` in `CoD3RecompLib/private/default.xex`. From the repo root, run:

```powershell
.\scripts\recompile.ps1 -BuildTools -BuildLib -BuildHost
```

Then run the built `CoD3.exe` from its output directory. On first launch, setup can use a disc image or an extracted game folder. To point it straight at an ISO, run this **from the executable's folder**:

```powershell
.\CoD3.exe --iso "D:\path\Call of Duty 3.iso"
```

These commands come from the existing upstream build script and README. I haven't freshly verified a clean build on every machine, so I won't claim that every setup is verified. More details are in the [archived build instructions](docs/UPSTREAM-README.md#build) and [STATUS.md](STATUS.md).

## AI disclosure, just being clear

**This project was assisted with Codex and playtested by me.** You're free to dislike AI-assisted development, but I'd rather be transparent about my tools than pretend I didn't use them. Also this README was also written by Codex, haha. But this wasn't. Neither this. I'm not lying. I'm just wasting your time.

I still decide what goes into my fork, and I actually play the damn game.

Original game assets are not provided. Upstream code and third-party components keep their own attribution and licensing requirements. This README doesn't magically relicense anybody else's work.

This is an experimental fork, not a polished, bug-free release. I'm improving a game I wanted to play on PC and documenting the bugs instead of hiding them.
