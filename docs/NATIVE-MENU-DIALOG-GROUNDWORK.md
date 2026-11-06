# Native menu/dialog groundwork (November 2026)

This is a **source-backed record** of how the main-menu Quit Game confirmation was implemented in COD3RECOMP-ENHANCED. It is intended to preserve the groundwork for future native-menu additions, **not** to claim the project already has a general-purpose menu framework.

## Provenance and status

- Feature commit: [`5da18f4` — Add main menu quit confirmation](https://github.com/Whitetigercub/COD3RECOMP-ENHANCED/commit/5da18f444e9470e19382196a57875132afd48c2d).
- Relevant files: [`CoD3Host/native_menu_pointer.cpp`](../CoD3Host/native_menu_pointer.cpp), [`CoD3Host/native_menu_pointer.h`](../CoD3Host/native_menu_pointer.h), [`CoD3Host/input.cpp`](../CoD3Host/input.cpp), and [`tests/native_menu_pointer_test.cpp`](../tests/native_menu_pointer_test.cpp).
- A focused host-pointer test passed during development, modified input compiled, and the full isolated build was reported successful. The user subsequently reported that the **Quit Game prompt works** in a live playtest. This is not a record of independently verified coverage for every Quit/Cancel/Esc/controller path.
- Earlier Windows Task Dialog patch was rejected: it did not apply cleanly and bypassed the native menu/pointer path. Do **not** use it as the basis for additional native menus.

## Existing native menu and modal path

- The pointer integration resolves the native UI manager at `0x82A8DF2C`, frontend and pause systems, and the modal slot. An active modal owns input and blocks underlying menu pointer actions, even if the modal itself cannot be safely interacted with.
- The frontend main-menu guard (`MainReady`) requires an owned, ready, non-modal frontend menu, current frontend index `1`, and the expected main-menu vtable `0x82071DDC`. This limits where a quit prompt can be requested.
- The modal creation path checks the expected modal vtable `0x820719D4`, an idle modal slot, the two expected native menu objects (vtable `0x820718EC`), valid presentation pointers and available guest functions **before** configuring the dialog.
- `RequestQuit()` queues a flag from input polling, instead of mutating guest UI state immediately. The queue is consumed by `Update()` at the native UI frame hook (`sub_82511168`), where guest callbacks can be invoked with the existing PPC context.

## Dialog setup and input safeguards

- `OpenQuit()` mirrors the native two-button setup identified at guest function `0x823FC2C8`. It uses the game's own dialog creation/button paths (`0x824D31E0`, `0x824C4428`, `0x824BDDE8`, `0x824CDB28`).
- The strings **Quit Game?**, **Quit**, and **Cancel** live in guest-allocated physical memory rather than a temporary host stack buffer. This avoids handing guest UI code pointers to ephemeral text.
- The initial selection is **Cancel** (row `1`). Quit is row `0`. The quit callback is scoped to the specific `quitMenu`; it requests the host window's existing `WM_CLOSE` shutdown route. Other dialogs use their original guest behavior.
- Native Back action `128` is routed explicitly to Cancel for **this dialog**, even if Quit is highlighted (`sub_824C48B8` wrapper). The normal confirmation dispatcher is wrapped by `sub_824CDD78`.
- In `input.cpp`, `quitOpeningKey` suppresses the same held Escape from also generating the frontend B/Back input that could close or act on the newly opened dialog. The latch clears after releasing Escape.
- The dialog-reuse safeguards track `quitMenu` and `configuringQuit`, reset stale ownership when a modal changes, and bump `modalRevision`/cancel in-flight pointer presses when native dialogs are configured. This is important because the guest can reuse the same pooled dialog address.

These addresses are tied to the **current Xbox 360 recompilation build**. Verify them again if regenerated guest code or upstream versions change.

## What the focused test actually covers

`tests/native_menu_pointer_test.cpp` runs the real host pointer module with **simulated guest memory and callbacks**. It exercises ordinary menu clicks, blank-space handling, modal selection and ownership, pointer cancellation/reconfiguration, pause routing, and slider dispatch. The quit-specific extension checks that `RequestQuit()` opens on the ready main menu, labels/rows are configured, Cancel is the default, Back dispatches Cancel (row 1), and a second quit request is blocked while the dialog is active.

The test harness **does not launch the game, draw the actual dialog, or validate all physical keyboard/controller sequences**. A full build plus user playtesting remains the release gate for changes to input/menu transitions.

## How to reuse the groundwork safely

For a future native confirmation, first identify the correct *page-eligibility condition*, reuse the same native modal ownership and callback mechanisms, and give that new dialog its **own identity and action routing**. Keep guest strings alive for as long as the modal may reference them. Set the safe choice as default, consume the triggering input so it cannot leak through, and invalidate old pointer presses on modal reuse.

Do **not** generalize the quit-specific `WM_CLOSE` behavior, current guest addresses, or row numbers into a blind all-purpose helper. Add focused tests for opening, confirm, cancel/back, repeated open/close, transitions, and interference with unrelated dialogs; then playtest title gateway, menus, controller navigation, and in-mission pause.

Known separate menu issues at the time of writing include the unresponsive brightness mouse slider and sensitivity sliders that can repeat after a click. The historical return-to-**PRESS ENTER** issue is struck out in the main README per latest project tracking; do not infer its fix solely from this dialog implementation.
