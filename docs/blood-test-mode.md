# Blood Test Mode

Developer-only diagnostics for repeatable wall/floor blood rendering. This is a testing mode, **not a proven fix** for the intermittent disappearing-decal bug.

## Enable and use

Build the normal `CoD3` target. Run a separate test copy of the executable, with copied settings/saves and access to the existing extracted game directory. Enable `COD3_BLOOD_TEST_MODE=1`, or place an empty `BloodTestMode.enable` beside the test executable. Neither opt-in is present in an ordinary installation.

Load a checkpoint, wait until the level is fully visible, then press **F7**. Select floor or aimed-wall placement under **F11 → Gameplay → Blood & Gore**. No enemy or shot is needed. An automatic initial capture may coincide with loading; discard that view and recapture with F7.

| Control | Action |
|---|---|
| F5 | Toggle normal versus depth-disabled rendering |
| F6 | Exit/re-enable developer mode |
| F7 | Capture a static surface and a new render snapshot |
| F8 | Clear the synthetic decal, retaining its anchor/snapshot |
| F9 | Regenerate identical geometry at the retained coordinates |
| F12 | Screenshot including diagnostics while developer mode is active |
| F11 | Switch frozen/live mode, floor/wall placement, and logging |

The persistent HUD is read-only; use the function keys or the F11 panel. Its GPU coverage numbers are sample counts, not a final-image visibility verdict. Diagnostic controls do not save changes to normal blood settings.

## Rendering and depth validation

The deterministic fixture is one 36-vertex fan, radius 8 world units, offset 0.04 along the surface normal, without lifetime expiration. It bypasses impacts and the gameplay decal pool, but uses the existing `BloodRender::Draw` shader, vertex layout, blend/rasterizer state, and `GREATER_EQUAL`/no-depth-write state.

Frozen mode retains the selected native projection and copies color plus depth/stencil resources. It checks dimensions/MSAA, retains the original DSV format, and compares normal/depth-disabled GPU coverage on the original source and copied depth at capture. Failures are displayed explicitly. Multisampled color copies retain render-target binding, required by D3D11.

Each frozen frame restores the same baseline color and draws the same fixture against the same depth/projection. Window resizing changes presentation size. F7 explicitly recaptures a changed native resolution. The snapshot is retained independently of underlying simulation resets.

Live mode inserts the same fixed fixture through the existing blood rendering path and uses the current native scene depth. It is the mode for native camera movement, geometry occlusion and render-order investigations.

## Known limits

- Frozen mode does not pause or replay level logic. The game can continue or fail behind the snapshot.
- Source/copy parity is measured at snapshot capture/presentation. It does not compare earlier blood-insertion depth with every subsequent native depth mutation.
- Frozen presentation does not replay native transparency, foreground/viewmodel layering, clears or later draw order. Use live mode for those conclusions.
- The inherited projection-selection heuristic remains unproven as a complete fix. Matching copies cannot independently establish correct engine-camera semantics.
- Surface capture is gated until a native level module and a recent world pass exist. Placement uses the existing static-world collision trace; no map loader or game assets are changed.
- Legacy diagnostic flags are independent. For this mode, leave `COD3_BLOOD_DIAG`, `COD3_BLOOD_DIAG_TEST`, `COD3_BLOOD_CONTROLLED` and `COD3_BLOOD_CAPTURE_DIR` unset to avoid the earlier experimental instrumentation.

## Verification recorded on 2026-11-04

- `CoD3` build succeeded.
- 156 WARP graphics/fixture checks passed, plus a separate opt-out check and 27 existing normal-blood GPU checks.
- Tests cover clear/regenerate determinism, 120 frozen frames, isolation from live camera/depth mutations, source/copy parity, known occlusion and the depth toggle, resource-size recapture, 4× MSAA with COD3-like typeless depth/stencil, and reset behavior.
- Native Chambois snapshot frame 8253, near 6 / far about 8191.34: source and copy both produced **740 normal / 740 depth-disabled samples**.
- The same frozen generation logged unchanged 740/740 coverage through 205.625 seconds. Images 144 seconds apart were identical outside the HUD; regeneration restored that same image.
- Native live mode showed both positive coverage and normal-depth rejection with positive no-depth coverage under changing combat/camera conditions. These are not proof of incorrect depth rejection.
- Mode exit restored ordinary rendering. The user subsequently reported blood remained visible after checkpoint reload; that observation does not establish the intermittent bug's cause.

The controlled frozen comparison did **not** reproduce the original disappearance. No normal-blood rendering fix is claimed. Native unobstructed visible-to-missing transitions and native render-resolution changes remain future validation work.

## Running the asset-independent regression

From a Visual Studio developer PowerShell with `clang-cl` available, at the repository root:

```powershell
$repo = (Get-Location).Path
$sources = @(
  'tests/blood_test_mode_test.cpp',
  'CoD3Host/blood_test_mode.cpp', 'CoD3Host/blood_test_gpu.cpp',
  'CoD3Host/blood_render.cpp',
  'thirdparty/imgui/imgui.cpp', 'thirdparty/imgui/imgui_draw.cpp',
  'thirdparty/imgui/imgui_tables.cpp', 'thirdparty/imgui/imgui_widgets.cpp'
) | ForEach-Object { Join-Path $repo $_ }
New-Item -ItemType Directory -Force build/blood-test-regression | Out-Null
Push-Location build/blood-test-regression
try {
  & clang-cl /nologo /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /std:c++20 /EHsc /O2 `
    "/I$repo/CoD3Host" "/I$repo/thirdparty/imgui" @sources `
    /Fe:blood_test_mode_test.exe /link d3d11.lib d3dcompiler.lib
  if ($LASTEXITCODE -ne 0) { throw 'Regression build failed' }
  & ./blood_test_mode_test.exe
  if ($LASTEXITCODE -ne 0) { throw 'Graphics regression failed' }
  & ./blood_test_mode_test.exe --disabled
  if ($LASTEXITCODE -ne 0) { throw 'Opt-out regression failed' }
} finally { Pop-Location }
```

The regression stubs native collision and uses WARP; native checkpoint validation is separate. No disc data, generated game code, binaries, user saves or captured assets are required by this test source.
