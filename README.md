# <img src="cod3recomplogo3.png" alt="COD3RECOMP-ENHANCED logo" width="600">


**Call of Duty 3 (Xbox 360) on PC, with the fixes and improvements I want.**

This is my enhanced fork of CoD3Recomp, based on upstream **0.5.424**. My main focus is the **single-player campaign**: fixing annoyances and adding a few modern touches.

The game itself is **NOT INCLUDED**. You need your own Xbox 360 version of Call of Duty 3. Legal shit, I know.

If you want the technical write-up, [here's the formal README](docs/README-FORMAL-BACKUP.md).

## Credit where it's due

**[FrankHUN88453/CoD3Recomp](https://github.com/FrankHUN88453/CoD3Recomp) - [hovg-N8/CoD3Recomp](https://github.com/hovg-N8/CoD3Recomp) - [my fork](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED)**

Frank started the recompilation project and PC renderer. hovg-N8 built extensively on top of it through 0.5.424. I didn't make their work and I'm not going to pretend I did.

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
- Added **experimental blood decals and adjustable blood clouds**. F11 - Gameplay - Blood & Gore. **CURRENTLY UNSTABLE.**
- Added experimental blood-decal projection filtering and rendering diagnostics.

## Known menu bugs — still need fixing

The mouse now points at actual menu entries. These issues have been tracked:

- The game's own **brightness slider does not respond** to the mouse.
- **Sensitivity sliders can keep changing after one click.**
- ~~Go from the title screen into the menus, then back to **PRESS ENTER**, and **Enter stops working**.~~ (Crossed off the active list after the latest menu update; retained here for reference.)

The brightness and sensitivity issues remain open. The crossed-out title-gateway item is retained as a historical report. Mouse hover and clicks have been playtested successfully in the Single Player and in-mission menus, plus other tested menus. The confirmed Single Player build and the expanded playtest executable are preserved locally. Later help-caption/button-capture refinements in this source have not been live playtested. Campaign build and focused simulated-input checks passed, but the tests missed the native slider-repeat behavior; they aren't proof that the bugs above are solved. Full gameplay cursor-capture transitions and multiplayer remain unverified.

See the [menu preview status](docs/MENU-POINTER-PREVIEW.md) and [bug log with reproduction/source leads](docs/MENU-SLIDER-BUGS.md). The slider fixes are deferred for a small follow-up patch.

## KNOWN ISSUE: blood decals sometimes disappear

The experimental blood decals remain **unreliable**. Sometimes they're visible, sometimes they disappear. The exact cause is **STILL UNKNOWN**.

Here's what we actually know: real decals have been generated, and visible pixel writes have been confirmed. In a separate diagnostic build, a **synthetic test decal stayed visible for over three minutes**. The frozen source/copy depth coverage matched at **740/740 samples**, with **156 diagnostic checks and 27 normal-renderer checks passed**.

I've also seen blood stay on the ground after reloading a checkpoint. Nice, but **that does NOT mean the bug is fixed**.

In live rendering, normal depth testing has rejected some decal coverage while depth-disabled rendering showed coverage. That could be legitimate occlusion. It is **not proof** that depth testing is broken.

**Current suspects, NOT confirmed causes:** depth or camera-projection mismatch, depth-buffer timing, later render passes covering the blood, first-person weapon/foreground layering, resource-state issues, or MSAA interactions.

### Update: live blood comparison finished (November 2026)

Sol ran the same-frame comparison. **24 paired frames** were captured with matching fixture/projection hashes, and every measured draw call succeeded. **Seven frames had TWO blood insertions**, so counting each actual draw matters.

For **five consecutive live frames**, the test stain passed **317 normal-depth and 317 depth-disabled samples** at both the early and late checks, and **102 RGB pixels actually changed** in each frame. In other words, blood *can* draw correctly and stay there through the sampled rendering stages.

Other samples failed the **initial depth test** even though depth-disabled coverage was positive. That's a real observation, but **it does NOT prove the rejection was wrong**. Some captures involved foreground overlap or the Mission Failed screen, so those aren't evidence of blood mysteriously vanishing during normal gameplay.

The game sometimes copies the scene to a separate texture for display. Sol checked that the changed pixels fell inside the copied region and matched the selected presentation texture. **No later pass erasing blood was proven.**

The latest comparison build passed **168 diagnostic checks**, the developer-mode opt-out check, and **27 normal-renderer checks**. The installed executable/settings weren't changed. **The comparison code and patch are still local/archived, not committed or pushed to GitHub yet.**

### Update: controlled campaign blood test (November 2026)

Phase 1 captured a real player hit producing **13 ground decals** during approximately **195 seconds** of gameplay. All **36 capture sets** showed blood drawing, with no logged decal deletion and a prominent stain remaining visible in sampled final images. The camera moved and the captures were spaced apart, so brief disappearances remain possible. **The original disappearing-blood bug was not reproduced and remains unresolved.**

An isolated Phase 2 gray-surface prototype is being explored separately. It is not a finished standalone room or a verified fix. Neither test package has been merged into this branch.

**The actual bug is STILL UNSOLVED.** The earlier proposed campaign-map comparison is now deferred in favor of the architecture reassessment described below.

## Blood renderer: architecture reassessment (planned, on hold)

The experimental blood decals are **disabled by default**, and their intermittent disappearance remains **unresolved**. A read-only, source-based review found that the hit/droplet/decal simulation appears broadly reasonable, but the rendering integration relies on **draw-order heuristics** and selecting a projection matrix from shader constants on a qualifying draw. These are **potential architectural weaknesses, not confirmed causes** of the missing stains. Previous synthetic and controlled tests showed decals can render, but did not establish reliable insertion during normal campaign rendering.

**Next step, only when development resumes:** Have Sol independently inspect the actual code and rendering pipeline from first principles. Verify the guest trace calling convention, the view/projection source, depth state, and where world draws and in-game decals occur. Propose a deterministic rendering strategy, with a clear explanation of what can be safely reused from the existing simulation, **before implementing changes**. If a redesign is approved, validate it on representative campaign maps with a bounded test plan. If it remains unreliable, keep the effect off and shelve it rather than repeating endless diagnostics.

**Mandatory multi-map acceptance test:** No blood-renderer fix can be considered verified from a single map, synthetic test, frozen scene, or successful draw call alone. Before calling it fixed, test normal gameplay on **multiple single-player campaign maps**, including **Chambois** (confirmed disappearing-blood case) and **Forest** (preliminary comparison), plus maps with different terrain and surfaces. Use real gunfire, camera movement, and both **Permanent decals ON and OFF**. Track individual stains and distinguish unexpected disappearance from intended expiration, fading, occlusion, or FIFO pool replacement. If Chambois still fails, the fix is **not verified**, regardless of passing diagnostic checks.

**No campaign-wide blood survey or further diagnostic run is currently required.** The plans below are retained as historical investigation notes, not active instructions.

## Archived plan: campaign-wide blood survey (formerly Plan C)

The earlier, now-deferred proposal was to test **every available single-player campaign map twice**:

1. **Permanent decals OFF:** use normal decal lifetime/fading and observe whether blood disappears.
2. **Permanent decals ON:** disable lifetime expiry and observe whether blood still disappears.

Keep the decal pool at **4,096** for comparable runs where the setting is available. Permanent ON disables expiry, but it does **not** prevent FIFO pool replacement, so record any pool-replacement events separately.

For every run, record the map, Permanent setting, whether a real player hit created custom decals, whether/when the same stain disappeared, and any lifetime or pool-replacement log entries. Keep screenshots and the complete run log. Use the same kind of unobstructed surface and a steady view where practical. If a map cannot be loaded or a run is inconclusive, mark it as such rather than counting it as a pass.

**Chambois is the known-bug map.** This is where the disappearing-blood problem has been observed, and it must be treated as a confirmed problem area rather than a neutral control map.

**Forest has behaved normally so far:** the blood remained visible during the recent test. This is preliminary, not a clean bill of health; Forest needs closer and repeated testing, including both Permanent-decals settings.

The remaining campaign maps still need the same ON/OFF comparison. The purpose of this survey is to establish whether Chambois is uniquely affected or whether other maps also reproduce the problem.

## Plan B: gray testing surface (on hold)

A developer-only gray-surface prototype using existing campaign geometry has been explored, but the first live version produced corrupted visuals and no usable weapon/fire input. A later repair removed the broad shader override and uses a small manually placed ground patch; its placement and equivalence still need live verification. Keep this plan on hold until the campaign-wide ON/OFF survey gives us a reason to resume it.

## Plan A: targeted renderer investigation (on hold)

Return to targeted renderer diagnostics or fixes only after the map survey provides evidence pointing to a shared rendering issue or a specific map-dependent path. Do not treat a synthetic/frozen decal test as proof that real gameplay decals cannot disappear.

For now, if the blood effects bother you, disable them in **F11 - Gameplay - Blood & Gore**.

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
