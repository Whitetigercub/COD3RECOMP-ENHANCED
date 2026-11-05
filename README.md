# COD3RECOMP-ENHANCED

<img src="cod3recomplogo3.png" alt="COD3RECOMP-ENHANCED logo" width="600">

An enhanced **Call of Duty 3 (Xbox 360) PC recompilation** fork focused on campaign fixes, quality-of-life changes, and experimental blood effects. The upstream codebase is based on version **0.5.424**.

## Project lineage and credits

**[FrankHUN88453/CoD3Recomp](https://github.com/FrankHUN88453/CoD3Recomp) → [hovg-N8/CoD3Recomp](https://github.com/hovg-N8/CoD3Recomp) → COD3RECOMP-ENHANCED (this fork).**

FrankHUN88453 created the original recompilation project, including its host runtime and Direct3D 11 renderer. hovg-N8 built on that work through version 0.5.424, adding major campaign, multiplayer, and performance improvements. This fork adds the changes listed separately below. Credit for inherited features belongs to their original contributors.

The earlier upstream READMEs are preserved in [docs/UPSTREAM-README.md](docs/UPSTREAM-README.md). For detailed upstream development history, see [docs/PATCH-NOTES.md](docs/PATCH-NOTES.md).

## Features inherited from hovg-N8 (not added by this fork)

The upstream 0.5.424 README describes a playable campaign, multiplayer over System Link (LAN or Tailscale), bots and custom classes, expanded player counts, killstreaks and killcam, experimental co-op, HDR/RenoDX options, texture enhancements, and performance work.

**Multiplayer is inherited from hovg-N8; it has not been tested by the maintainer of COD3RECOMP-ENHANCED.** Upstream functionality and claims should not be mistaken for independent validation in this fork.

## Changes added in COD3RECOMP-ENHANCED

- Fixed three broken actions: **Reload, Binoculars, and Objectives**.
- Added mouse-and-keyboard button prompts in the interface.
- Added modern-style hitmarkers with optional sound and settings in **F11 → Gameplay**.
- Added an **M1 Garand partial reload**.
- Added **experimental blood decals and adjustable blood clouds**, configurable in **F11 → Gameplay → Blood & Gore**. This feature is currently unstable.
- Added experimental blood-decal projection filtering and rendering diagnostics.

These are the enhanced fork's changes; the much larger upstream feature set above was not authored here. This fork has not been given a separate numbered release version.

## Quick start / build from source (Windows)

You must supply your **own Xbox 360 version** of Call of Duty 3. No game content, precompiled game executable, or copyrighted disc image is provided by this repository.

The upstream build instructions call for **CMake 3.20+**, **Ninja**, **Clang**, and **Visual Studio Build Tools with Windows SDK headers**. Clang is required for the recompilation toolchain.

Place a `default.xex` from your own Xbox 360 game dump at `CoD3RecompLib/private/default.xex`, then run this command from the repository root in PowerShell:

```powershell
.\scripts\recompile.ps1 -BuildTools -BuildLib -BuildHost
```

Once built, launch `CoD3.exe` from its output location. If there is no `game` folder beside the executable, the first-run setup can use an Xbox 360 disc image or an extracted game directory. To specify an image from the command line:

```powershell
.\CoD3.exe --iso "D:\path\Call of Duty 3.iso"
```

The launch command above assumes PowerShell is running in the directory containing `CoD3.exe`. See the [archived upstream instructions](docs/UPSTREAM-README.md#build) for detailed options and [STATUS.md](STATUS.md) for upstream implementation notes. These instructions are documented from the repository's build script and prior README, not a fresh build verification.

## Known bug: intermittent blood decal disappearance

**Status: Unresolved. No confirmed root cause or permanent fix.**

Experimental blood decals sometimes disappear or become invisible during live gameplay. Decal generation and visible color writes have been observed, but the intermittent disappearance has not been reproduced in a controlled test.

**What has actually been validated:** A deterministic synthetic blood decal remained stable in a frozen rendering test for more than three minutes. The source and copied depth produced matching coverage (**740/740 samples**), with **156 diagnostic checks and 27 ordinary-renderer checks passing**. A user also observed blood remaining visible after a checkpoint reload, but this alone does not prove the bug is fixed.

During live rendering, normal depth testing sometimes rejected the decal while depth-disabled rendering showed positive coverage. Because the scene and camera were changing, that is **not proof that depth rejection was incorrect**.

**Possible causes, not confirmed:** a depth or projection mismatch, timing of depth-buffer updates, another rendering pass overwriting or obscuring the decal, foreground/viewmodel layering, render-state/resource binding issues, or interactions with MSAA. None has been isolated as the cause.

**Next targeted investigation:** Measure the *same synthetic decal* immediately after its normal draw and after final scene rendering **in one live frame**, with identical geometry, projection, and frame ID. Compare both depth coverage and actual color-buffer writes before changing the gameplay renderer.

Blood effects can be disabled under **F11 → Gameplay → Blood & Gore** if they cause trouble.

## Scope and disclosure

Development and personal playtesting of this enhanced fork are focused on the **single-player campaign**. Multiplayer functionality is inherited from upstream and **has not been tested here**.

This project was assisted with Codex and playtested by me. You're free to dislike AI-assisted development, but I'd rather be transparent about my tools than pretend I didn't use them.

This repository does not contain the original game assets. Inherited code and third-party components retain their respective attribution and licensing requirements; no new blanket license is asserted here.
