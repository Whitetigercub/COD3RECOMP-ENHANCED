<img src="cod3recomplogo3.png" alt="COD3RECOMP-ENHANCED logo" width="600">

# COD3RECOMP-ENHANCED

**Call of Duty 3 (Xbox 360) on PC, with some shit I wanted fixed.**

This is my enhanced fork of CoD3Recomp, based on upstream **0.5.424**. I mainly give a shit about the **single-player campaign**. I'm fixing annoyances, adding a few modern touches, and occasionally fighting the renderer because apparently the blood decals have a personal vendetta against me.

The game itself is **NOT INCLUDED**. You need your own Xbox 360 version of Call of Duty 3.

## Credit where it's due

**[FrankHUN88453/CoD3Recomp](https://github.com/FrankHUN88453/CoD3Recomp) → [hovg-N8/CoD3Recomp](https://github.com/hovg-N8/CoD3Recomp) → [my fork](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED)**

Frank started the recompilation project and PC renderer. hovg-N8 built a hell of a lot on top of it through 0.5.424. I didn't make their work and I'm not going to pretend I did.

The upstream project already has major campaign improvements, multiplayer features, bots, killstreaks, HDR options, performance changes, and more. **Those are NOT my additions.**

And to be clear, **I HAVE NOT TESTED MULTIPLAYER.** hovg-N8's fork has multiplayer functionality, but I'm working on the campaign, so I'm not going to claim I've personally verified that shit.

I've kept the older upstream READMEs in [docs/UPSTREAM-README.md](docs/UPSTREAM-README.md), and their big development history is in [docs/PATCH-NOTES.md](docs/PATCH-NOTES.md). No one's credits got thrown in the trash.

## What I actually changed

- **Fixed three broken buttons:** RELOAD, BINOCULARS, and OBJECTIVES. Yes, basic buttons. They were busted.
- Added **mouse and keyboard button icons/prompts** to the UI.
- Added **modern COD-style hitmarkers**, with sound and customization. F11 → Gameplay.
- Added **M1 Garand partial reload**, because throwing away the whole clip behavior every time wasn't what I wanted.
- Added **experimental blood decals and adjustable blood clouds**. F11 → Gameplay → Blood & Gore. **CURRENTLY UNSTABLE.**
- Added experimental blood-decal projection filtering and rendering diagnostics.

Nothing here is supposed to rewrite the entire game. I wanted the damn campaign to feel better on PC.

## THE KNOWN BUG: BLOOD DECALS KEEP FUCKING DISAPPEARING

I'm not going to sugarcoat this: the experimental blood decals are **buggy as hell**. Sometimes they're visible, sometimes they disappear. The exact cause is **STILL UNKNOWN**.

Here's what we actually know: real decals have been generated, and visible pixel writes have been confirmed. In a separate diagnostic build, a **synthetic test decal stayed visible for over three minutes**. The frozen source/copy depth coverage matched at **740/740 samples**, with **156 diagnostic checks and 27 normal-renderer checks passed**.

I've also seen blood stay on the ground after reloading a checkpoint. Nice, but **that does NOT mean the bug is fixed**.

In live rendering, normal depth testing has rejected some decal coverage while depth-disabled rendering showed coverage. That could be legitimate occlusion. It is **not proof** that depth testing is broken.

**Current suspects, NOT confirmed causes:** depth or camera-projection mismatch, depth-buffer timing, later render passes covering the blood, first-person weapon/foreground layering, resource-state issues, or MSAA interactions.

**Next test:** take the **same synthetic decal in the same live frame**, inspect it immediately after its blood draw, then inspect the final rendered scene. Same geometry, projection, and frame ID. If the pixels are there at first but gone at the end, something later may be covering them. If they never write, investigate the draw/depth path. **No random fixes until there's evidence.**

For now, if the blood effects piss you off too, disable them in **F11 → Gameplay → Blood & Gore**.

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

These commands come from the existing upstream build script and README. I haven't freshly verified a clean build on every machine, so I'm not going to bullshit you about that. More details are in the [archived build instructions](docs/UPSTREAM-README.md#build) and [STATUS.md](STATUS.md).

## AI disclosure, because I won't pretend otherwise

**This project was assisted with Codex and playtested by me.** You're free to dislike AI-assisted development, but I'd rather be transparent about my tools than pretend I didn't use them.

I still decide what goes into my fork, and I actually play the damn game.

Original game assets are not provided. Upstream code and third-party components keep their own attribution and licensing requirements. This README doesn't magically relicense anybody else's work.

If you're looking for some polished, bug-free miracle port, **this ain't it yet**. I'm tinkering with a game I wanted to play on PC, fixing things as I go, and documenting the bugs instead of hiding them.
